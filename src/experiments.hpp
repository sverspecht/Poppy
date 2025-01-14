#ifndef SRC_EXPERIMENTS_HPP_
#define SRC_EXPERIMENTS_HPP_

#include <opencv2/calib3d/calib3d.hpp>
#include "algo.hpp"
#include "util.hpp"
#include "matcher.hpp"


namespace poppy {
using namespace cv;
using namespace std;

int ratioTest(std::vector<std::vector<cv::DMatch> >
		&matches) {
	int removed = 0;
	// for all matches
	for (std::vector<std::vector<cv::DMatch> >::iterator
	matchIterator = matches.begin();
			matchIterator != matches.end(); ++matchIterator) {
		// if 2 NN has been identified
		if (matchIterator->size() > 1) {
			// check distance ratio
			if ((*matchIterator)[0].distance /
					(*matchIterator)[1].distance > 0.7) {
				matchIterator->clear(); // remove match
				removed++;
			}
		} else { // does not have 2 neighbours
			matchIterator->clear(); // remove match
			removed++;
		}
	}
	return removed;
}

cv::Mat ransacTest(
		const std::vector<cv::DMatch> &matches,
		const std::vector<cv::KeyPoint> &keypoints1,
		const std::vector<cv::KeyPoint> &keypoints2,
		std::vector<cv::DMatch> &outMatches)
		{
	// Convert keypoints into Point2f
	std::vector<cv::Point2f> points1, points2;
	for (std::vector<cv::DMatch>::
	const_iterator it = matches.begin();
			it != matches.end(); ++it) {
		// Get the position of left keypoints
		float x = keypoints1[it->queryIdx].pt.x;
		float y = keypoints1[it->queryIdx].pt.y;
		points1.push_back(cv::Point2f(x, y));
		// Get the position of right keypoints
		x = keypoints2[it->trainIdx].pt.x;
		y = keypoints2[it->trainIdx].pt.y;
		points2.push_back(cv::Point2f(x, y));
	}
	// Compute F matrix using RANSAC
	std::vector<uchar> inliers(points1.size(), 0);
	std::vector<cv::Point2f> out;
	//cv::Mat fundemental= cv::findFundamentalMat(points1, points2, out, CV_FM_RANSAC, 3, 0.99);

	cv::Mat fundemental = findFundamentalMat(
			cv::Mat(points1), cv::Mat(points2), // matching points
			inliers,      // match status (inlier or outlier)
			FM_RANSAC, // RANSAC method
			3.0,     // distance to epipolar line
			0.99);  // confidence probability

	// extract the surviving (inliers) matches
	std::vector<uchar>::const_iterator
	itIn = inliers.begin();
	std::vector<cv::DMatch>::const_iterator
	itM = matches.begin();
	// for all matches
	for (; itIn != inliers.end(); ++itIn, ++itM) {
		if (*itIn) { // it is a valid match
			outMatches.push_back(*itM);
		}
	}
	return fundemental;
}

void mergeMatches(
		const std::vector<std::vector<cv::DMatch>> &matches1,
		const std::vector<std::vector<cv::DMatch>> &matches2,
		std::vector<cv::DMatch> &symMatches) {
	// for all matches image 1 -> image 2
	for (std::vector<std::vector<cv::DMatch>>::
	const_iterator matchIterator1 = matches1.begin();
			matchIterator1 != matches1.end(); ++matchIterator1) {
		// ignore deleted matches
		if (matchIterator1->size() < 2)
			continue;
		// for all matches image 2 -> image 1
		for (std::vector<std::vector<cv::DMatch>>::
		const_iterator matchIterator2 = matches2.begin();
				matchIterator2 != matches2.end();
				++matchIterator2) {
			// ignore deleted matches
			if (matchIterator2->size() < 2)
				continue;
			symMatches.push_back(
					cv::DMatch((*matchIterator1)[0].queryIdx,
							(*matchIterator1)[0].trainIdx,
							(*matchIterator1)[0].distance));
			break; // next match in image 1 -> image 2
		}
	}
}

void symmetryTest(
		const std::vector<std::vector<cv::DMatch>> &matches1,
		const std::vector<std::vector<cv::DMatch>> &matches2,
		std::vector<cv::DMatch> &symMatches) {
	// for all matches image 1 -> image 2
	for (std::vector<std::vector<cv::DMatch>>::
	const_iterator matchIterator1 = matches1.begin();
			matchIterator1 != matches1.end(); ++matchIterator1) {
		// ignore deleted matches
		if (matchIterator1->size() < 2)
			continue;
		// for all matches image 2 -> image 1
		for (std::vector<std::vector<cv::DMatch>>::
		const_iterator matchIterator2 = matches2.begin();
				matchIterator2 != matches2.end();
				++matchIterator2) {
			// ignore deleted matches
			if (matchIterator2->size() < 2)
				continue;
			// Match symmetry test
			if ((*matchIterator1)[0].queryIdx ==
					(*matchIterator2)[0].trainIdx &&
					(*matchIterator2)[0].queryIdx ==
							(*matchIterator1)[0].trainIdx) {
				// add symmetrical match
				symMatches.push_back(
						cv::DMatch((*matchIterator1)[0].queryIdx,
								(*matchIterator1)[0].trainIdx,
								(*matchIterator1)[0].distance));
				break; // next match in image 1 -> image 2
			}
		}
	}
}


//double morph_distance_exp(const Mat &img1, const Mat &img2) {
//	Features ft1, ft2;
//	Matcher matcher(img1, img2, ft1, ft2);
//	Mat tmp1, tmp2;
//	if(img1.type() != CV_8UC1) {
//		cvtColor(img1, tmp1, COLOR_RGB2GRAY);
//	} else {
//		tmp1 = img1;
//	}
//
//	if(img2.type() != CV_8UC1) {
//		cvtColor(img2, tmp2, COLOR_RGB2GRAY);
//	} else {
//		tmp2 = img2;
//	}
//
//	vector<Point2f> corners1, corners2;
//	goodFeaturesToTrack(tmp1, corners1, 25,0.01,10);
//	goodFeaturesToTrack(tmp2, corners2, 25,0.01,10);
//
//	if(corners1.empty() || corners2.empty())
//		return numeric_limits<double>::max();
//
//	if (corners1.size() > corners2.size())
//		corners1.resize(corners2.size());
//	else
//		corners2.resize(corners1.size());
//
//
//	matcher.match(tmp1, tmp2, corners1, corners2);
//
//	if (corners1.size() > corners2.size())
//		corners1.resize(corners2.size());
//	else
//		corners2.resize(corners1.size());
//
//	return morph_distance(corners1, corners2, img1.cols, img1.rows);
//}

void fft_shift(const Mat &input_img, Mat &output_img)
{
	output_img = input_img.clone();
	int cx = output_img.cols / 2;
	int cy = output_img.rows / 2;
	Mat q1(output_img, Rect(0, 0, cx, cy));
	Mat q2(output_img, Rect(cx, 0, cx, cy));
	Mat q3(output_img, Rect(0, cy, cx, cy));
	Mat q4(output_img, Rect(cx, cy, cx, cy));

	Mat temp;
	q1.copyTo(temp);
	q4.copyTo(q1);
	temp.copyTo(q4);
	q2.copyTo(temp);
	q3.copyTo(q2);
	temp.copyTo(q3);
}


void calculate_dft(Mat &src, Mat &dst) {
	Mat padded;
	int m = getOptimalDFTSize( src.rows );
	int n = getOptimalDFTSize( src.cols ); // on the border add zero values
	copyMakeBorder(src, padded, 0, m - src.rows, 0, n - src.cols, BORDER_CONSTANT, Scalar::all(0));
	Mat z = Mat::zeros(m, n, CV_32F);
	Mat p;
	padded.convertTo(p, CV_32F);
	vector<Mat> planes = {p, z};
	Mat complexI;
	merge(planes, complexI);         // Add to the expanded another plane with zeros
	dft(complexI, complexI);         // this way the result may fit in the source matrix
	dst = complexI.clone();
}

Mat construct_H(Mat &scr, string type, float D0) {
	Mat H(scr.size(), CV_32F, Scalar(1));
	float D = 0;
	if (type == "Ideal") {
		for (int u = 0; u < H.rows; u++) {
			for (int  v = 0; v < H.cols; v++) {
				D = sqrt((u - scr.rows / 2)*(u - scr.rows / 2) + (v - scr.cols / 2)*(v - scr.cols / 2));
				if (D > D0)	{
					H.at<float>(u, v) = 0;
				}
			}
		}
		return (1.0 - H);
	}
	else if (type == "Gaussian") {
		for (int  u = 0; u < H.rows; u++) {
			for (int v = 0; v < H.cols; v++) {
				D = sqrt((u - scr.rows / 2)*(u - scr.rows / 2) + (v - scr.cols / 2)*(v - scr.cols / 2));
				H.at<float>(u, v) = exp(-D*D / (2 * D0*D0));
			}
		}
		return (1-0 - H);
	}
	assert(false);
	return H;
}


void fft_filter(Mat &scr, Mat &dst, Mat &H)
{
	fft_shift(H, H);
	Mat planesH[] = { Mat_<float>(H.clone()), Mat_<float>(H.clone()) };

	Mat planes_dft[] = { scr, Mat::zeros(scr.size(), CV_32F) };
	split(scr, planes_dft);

	Mat planes_out[] = { Mat::zeros(scr.size(), CV_32F), Mat::zeros(scr.size(), CV_32F) };
	cv::resize(planesH[0], planesH[0], planes_dft[0].size());
	cv::resize(planesH[1], planesH[1], planes_dft[1].size());
	planes_out[0] = planesH[0].mul(planes_dft[0]);
	planes_out[1] = planesH[1].mul(planes_dft[1]);

	merge(planes_out, 2, dst);

}

void dft_spectrum(const Mat& src, Mat& dst)
{
    Mat padded;                            //expand input image to optimal size
    int m = getOptimalDFTSize( src.rows );
    int n = getOptimalDFTSize( src.cols ); // on the border add zero values
    copyMakeBorder(src, padded, 0, m - src.rows, 0, n - src.cols, BORDER_CONSTANT, Scalar::all(0));
    Mat planes[] = {Mat_<float>(padded), Mat::zeros(padded.size(), CV_32F)};
    Mat complexI;
    merge(planes, 2, complexI);         // Add to the expanded another plane with zeros
    dft(complexI, complexI);            // this way the result may fit in the source matrix
    // compute the magnitude and switch to logarithmic scale
    // => log(1 + sqrt(Re(DFT(I))^2 + Im(DFT(src))^2))
    split(complexI, planes);                   // planes[0] = Re(DFT(I), planes[1] = Im(DFT(src))
    magnitude(planes[0], planes[1], planes[0]);// planes[0] = magnitude
    Mat magI = planes[0];
    magI += Scalar::all(1);                    // switch to logarithmic scale
    log(magI, magI);
    // crop the spectrum, if it has an odd number of rows or columns
    magI = magI(Rect(0, 0, magI.cols & -2, magI.rows & -2));
    // rearrange the quadrants of Fourier image  so that the origin is at the image center
    int cx = magI.cols/2;
    int cy = magI.rows/2;
    Mat q0(magI, Rect(0, 0, cx, cy));   // Top-Left - Create a ROI per quadrant
    Mat q1(magI, Rect(cx, 0, cx, cy));  // Top-Right
    Mat q2(magI, Rect(0, cy, cx, cy));  // Bottom-Left
    Mat q3(magI, Rect(cx, cy, cx, cy)); // Bottom-Right
    Mat tmp;                           // swap quadrants (Top-Left with Bottom-Right)
    q0.copyTo(tmp);
    q3.copyTo(q0);
    tmp.copyTo(q3);
    q1.copyTo(tmp);                    // swap quadrant (Top-Right with Bottom-Left)
    q2.copyTo(q1);
    tmp.copyTo(q2);
    normalize(magI, magI, 0, 1, NORM_MINMAX); // Transform the matrix with float values into a
                                           // viewable image form (float between values 0 and 1).
    dst = magI.clone();
}

double dft_detail2(const Mat& src, Mat& dst) {
	assert(!src.empty());
	Mat spectrum;
	dft_spectrum(src, spectrum);
	assert(!spectrum.empty());
	double powsum = 0;
	for(int c = 0; c < spectrum.cols; ++c) {
		for(int r = 0; r < spectrum.rows; ++r) {
			powsum += pow(double(spectrum.at<uchar>(r, c)), 2);
		}
	}
	dst = spectrum.clone();
	return sqrt(powsum / (spectrum.cols * spectrum.rows));
}

double dft_detail(const Mat& src, Mat& dst) {
	Mat imgIn;
	src.convertTo(imgIn, CV_32F);

	// DFT
	Mat DFT_image;
	calculate_dft(imgIn, DFT_image);
//	show_image("dft", DFT_image);
	DFT_image = cv::abs(DFT_image);

	// construct H
	Mat H;
	H = construct_H(imgIn, "Gaussian", 50);

	// filtering
	Mat complexIH;
	fft_filter(DFT_image, complexIH, H);

	// IDFT
	Mat imgOut;
	dft(complexIH, imgOut, DFT_INVERSE | DFT_REAL_OUTPUT);

	dst = imgOut.clone();
	return countNonZero(1.0 - imgOut);
}
}

#endif /* SRC_EXPERIMENTS_HPP_ */
