#include "algo.hpp"
#include "draw.hpp"
#include "util.hpp"
#include "blend.hpp"
#include "settings.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <algorithm>
#include <map>

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/videoio/videoio.hpp>
#include <opencv2/features2d.hpp>
#include <opencv2/core/ocl.hpp>
#include <opencv2/video/video.hpp>

using namespace std;
using namespace cv;
namespace poppy {

void canny_threshold(const Mat &src, Mat &detected_edges, double thresh) {
	detected_edges = src.clone();
	GaussianBlur(src, detected_edges, Size(9, 9), 1);
	Canny(detected_edges, detected_edges, thresh, thresh * 2);
}

void make_delaunay_mesh(const Size &size, Subdiv2D &subdiv, std::vector<Point2f> &dstPoints) {
	vector<Vec6f> triangleList;
	subdiv.getTriangleList(triangleList);
	vector<Point> pt(3);
	Rect rect(0, 0, size.width, size.height);

	for (size_t i = 0; i < triangleList.size(); i++) {
		Vec6f t = triangleList[i];
		pt[0] = Point(cvRound(t[0]), cvRound(t[1]));
		pt[1] = Point(cvRound(t[2]), cvRound(t[3]));
		pt[2] = Point(cvRound(t[4]), cvRound(t[5]));

		if (rect.contains(pt[0]) && rect.contains(pt[1]) && rect.contains(pt[2])) {
			dstPoints.push_back(pt[0]);
			dstPoints.push_back(pt[1]);
			dstPoints.push_back(pt[2]);
		}
	}
}

std::pair<std::vector<Point2f>, std::vector<Point2f>> find_matches(const Mat &grey1, const Mat &grey2) {
	if (Settings::instance().max_keypoints == -1)
		Settings::instance().max_keypoints = hypot(grey1.cols, grey1.rows) / 4.0;
	cv::Ptr<cv::ORB> detector = cv::ORB::create(Settings::instance().max_keypoints);
	cv::Ptr<cv::ORB> extractor = cv::ORB::create();

	std::vector<KeyPoint> keypoints1, keypoints2;

	Mat descriptors1, descriptors2;
	detector->detect(grey1, keypoints1);
	detector->detect(grey2, keypoints2);

	detector->compute(grey1, keypoints1, descriptors1);
	detector->compute(grey2, keypoints2, descriptors2);

	Mat matMatches;

	std::vector<Point2f> points1, points2;
	for (auto pt1 : keypoints1)
		points1.push_back(pt1.pt);

	for (auto pt2 : keypoints2)
		points2.push_back(pt2.pt);

	return {points1,points2};
}

cv::Mat points_to_homogenous_mat(const std::vector<cv::Point2f> &pts) {
	int numPts = pts.size();
	cv::Mat homMat(3, numPts, CV_32FC1);
	for (int i = 0; i < numPts; i++) {
		homMat.at<float>(0, i) = pts[i].x;
		homMat.at<float>(1, i) = pts[i].y;
		homMat.at<float>(2, i) = 1.0;
	}
	return homMat;
}

void morph_points(std::vector<cv::Point2f> &srcPts1, std::vector<cv::Point2f> &srcPts2, std::vector<cv::Point2f> &dstPts, float s) {
	assert(srcPts1.size() == srcPts2.size());

	int numPts = srcPts1.size();

	dstPts.resize(numPts);
	for (int i = 0; i < numPts; i++) {
		dstPts[i].x = (1.0 - s) * srcPts1[i].x + s * srcPts2[i].x;
		dstPts[i].y = (1.0 - s) * srcPts1[i].y + s * srcPts2[i].y;
	}
}

void get_triangle_indices(const cv::Subdiv2D &subDiv, const std::vector<cv::Point2f> &points, std::vector<cv::Vec3i> &triangleVertices) {
	std::vector<cv::Vec6f> triangles;
	subDiv.getTriangleList(triangles);

	int numTriangles = triangles.size();
	triangleVertices.clear();
	triangleVertices.reserve(numTriangles);
	for (int i = 0; i < numTriangles; i++) {
		std::vector<cv::Point2f>::const_iterator vert1, vert2, vert3;
		vert1 = std::find(points.begin(), points.end(), cv::Point2f(triangles[i][0], triangles[i][1]));
		vert2 = std::find(points.begin(), points.end(), cv::Point2f(triangles[i][2], triangles[i][3]));
		vert3 = std::find(points.begin(), points.end(), cv::Point2f(triangles[i][4], triangles[i][5]));

		cv::Vec3i vertex;
		if (vert1 != points.end() && vert2 != points.end() && vert3 != points.end()) {
			vertex[0] = vert1 - points.begin();
			vertex[1] = vert2 - points.begin();
			vertex[2] = vert3 - points.begin();
			triangleVertices.push_back(vertex);
		}
	}
}

void make_triangler_points(const std::vector<cv::Vec3i> &triangleVertices, const std::vector<cv::Point2f> &points, std::vector<std::vector<cv::Point2f>> &trianglerPts) {
	int numTriangles = triangleVertices.size();
	trianglerPts.resize(numTriangles);
	for (int i = 0; i < numTriangles; i++) {
		std::vector<cv::Point2f> triangle;
		for (int j = 0; j < 3; j++) {
			triangle.push_back(points[triangleVertices[i][j]]);
		}
		trianglerPts[i] = triangle;
	}
}

void paint_triangles(cv::Mat &img, const std::vector<std::vector<cv::Point2f>> &triangles) {
	int numTriangles = triangles.size();

	for (int i = 0; i < numTriangles; i++) {
		std::vector<cv::Point> poly(3);

		for (int j = 0; j < 3; j++) {
			poly[j] = cv::Point(cvRound(triangles[i][j].x), cvRound(triangles[i][j].y));
		}
		cv::fillConvexPoly(img, poly, cv::Scalar(i + 1));
	}
}

void solve_homography(const std::vector<cv::Point2f> &srcPts1, const std::vector<cv::Point2f> &srcPts2, cv::Mat &homography) {
	assert(srcPts1.size() == srcPts2.size());
	homography = points_to_homogenous_mat(srcPts2) * points_to_homogenous_mat(srcPts1).inv();
}

void solve_homography(const std::vector<std::vector<cv::Point2f>> &srcPts1,
		const std::vector<std::vector<cv::Point2f>> &srcPts2,
		std::vector<cv::Mat> &hmats) {
	assert(srcPts1.size() == srcPts2.size());

	int ptsNum = srcPts1.size();
	hmats.clear();
	hmats.reserve(ptsNum);
	for (int i = 0; i < ptsNum; i++) {
		cv::Mat homography;
		solve_homography(srcPts1[i], srcPts2[i], homography);
		hmats.push_back(homography);
	}
}

void morph_homography(const cv::Mat &Hom, cv::Mat &MorphHom1, cv::Mat &MorphHom2, float blend_ratio) {
	cv::Mat invHom = Hom.inv();
	MorphHom1 = cv::Mat::eye(3, 3, CV_32FC1) * (1.0 - blend_ratio) + Hom * blend_ratio;
	MorphHom2 = cv::Mat::eye(3, 3, CV_32FC1) * blend_ratio + invHom * (1.0 - blend_ratio);
}

void morph_homography(const std::vector<cv::Mat> &homs,
		std::vector<cv::Mat> &morphHoms1,
		std::vector<cv::Mat> &morphHoms2,
		float blend_ratio) {
	int numHoms = homs.size();
	morphHoms1.resize(numHoms);
	morphHoms2.resize(numHoms);
	for (int i = 0; i < numHoms; i++) {
		morph_homography(homs[i], morphHoms1[i], morphHoms2[i], blend_ratio);
	}
}

void create_map(const cv::Mat &triangleMap, const std::vector<cv::Mat> &homMatrices, cv::Mat &mapx, cv::Mat &mapy) {
	assert(triangleMap.type() == CV_32SC1);

	// Allocate cv::Mat for the map
	mapx.create(triangleMap.size(), CV_32FC1);
	mapy.create(triangleMap.size(), CV_32FC1);

	// Compute inverse matrices
	std::vector<cv::Mat> invHomMatrices(homMatrices.size());
	for (size_t i = 0; i < homMatrices.size(); i++) {
		invHomMatrices[i] = homMatrices[i].inv();
	}

	for (int y = 0; y < triangleMap.rows; y++) {
		for (int x = 0; x < triangleMap.cols; x++) {
			int idx = triangleMap.at<int>(y, x) - 1;
			if (idx >= 0) {
				cv::Mat H = invHomMatrices[triangleMap.at<int>(y, x) - 1];
				float z = H.at<float>(2, 0) * x + H.at<float>(2, 1) * y + H.at<float>(2, 2);
				if (z == 0)
					z = 0.00001;
				mapx.at<float>(y, x) = (H.at<float>(0, 0) * x + H.at<float>(0, 1) * y + H.at<float>(0, 2)) / z;
				mapy.at<float>(y, x) = (H.at<float>(1, 0) * x + H.at<float>(1, 1) * y + H.at<float>(1, 2)) / z;
			}
			else {
				mapx.at<float>(y, x) = x;
				mapy.at<float>(y, x) = y;
			}
		}
	}
}

void draw_contour_map(std::vector<std::vector<std::vector<cv::Point>>> &collected, vector<Vec4i> &hierarchy, Mat &dst, int cols, int rows, int type) {
	dst = Mat::zeros(rows, cols, type);

	for (size_t i = 0; i < collected.size(); ++i) {
		auto &contours = collected[i];
		double shade = 0;

		for (size_t j = 0; j < contours.size(); ++j) {
			shade = 32.0 + 223.0 * (double(j) / contours.size());
			cv::drawContours(dst, contours, j, { shade, shade, shade }, 1.0, cv::LINE_8, hierarchy, 0);
		}
	}
}

void find_contours(const Mat &img1, const Mat &img2, std::vector<Mat> &dst1, std::vector<Mat> &dst2, Mat &allContours1, Mat &allContours2, vector<vector<vector<Point>>>& collected1, vector<vector<vector<Point>>>& collected2) {
	Mat median1, median2, lap1, lap2, grey1, grey2;

	vector<Vec4i> hierarchy1;
	vector<Vec4i> hierarchy2;
	cvtColor(img1, grey1, cv::COLOR_RGB2GRAY);
	cvtColor(img2, grey2, cv::COLOR_RGB2GRAY);

	medianBlur(grey1, median1, 3);
	medianBlur(grey2, median2, 3);

	Laplacian(median1, lap1, median1.depth());
	Laplacian(median2, lap2, median2.depth());
	Mat sharp1 = grey1 - (0.7 * lap1);
	Mat sharp2 = grey2 - (0.7 * lap2);
	Mat eq1;
	Mat eq2;

	equalizeHist(sharp1, eq1);
	equalizeHist(sharp2, eq2);

	Mat fgMask1;
	Mat fgMask2;

	auto pBackSub1 = createBackgroundSubtractorMOG2(500, 16, true);
	medianBlur(eq1, median1, 3);
	pBackSub1->apply(eq1, fgMask1);
	pBackSub1->apply(median1, fgMask1);

	auto pBackSub2 = createBackgroundSubtractorMOG2(500, 16, true);
	medianBlur(eq2, median2, 3);
	pBackSub2->apply(eq2, fgMask2);
	pBackSub2->apply(median2, fgMask2);

	eq1 = median1 * 0.25 + fgMask1 * 0.75;
	eq2 = median2 * 0.25 + fgMask2 * 0.75;
	show_image("eq1", eq1);
	show_image("eq2", eq2);

	double t1 = 0, t2 = 0;
	Mat thresh1, thresh2;
	std::vector<std::vector<Point>> contourPoints1;
	for (off_t i = 0; i < 16; ++i) {
		t1 = std::max(0, std::min(255, (int) round(i * 16.0 * Settings::instance().contour_sensitivity)));
		t2 = std::max(0, std::min(255, (int) round((i + 1) * 16.0 * Settings::instance().contour_sensitivity)));
		cv::threshold(eq1, thresh1, t1, t2, 0);
		cv::findContours(thresh1, contourPoints1, hierarchy1, cv::RETR_TREE, cv::CHAIN_APPROX_TC89_KCOS);
		collected1.push_back(contourPoints1);
		contourPoints1.clear();
	}

	Mat cmap1, cmap2;
	draw_contour_map(collected1, hierarchy1, cmap1, grey1.cols, grey1.rows, grey1.type());
	show_image("cmap1", cmap1);

	size_t off = 0;
	std::vector<std::vector<Point>> contourPoints2;
	for (off_t j = 0; j < 16; ++j) {
		t1 = std::min(255, (int) round((off + j) * 16 * Settings::instance().contour_sensitivity));
		t2 = std::min(255, (int) round((off + j + 1) * 16 * Settings::instance().contour_sensitivity));
		cv::threshold(eq2, thresh2, t1, t2, 0);
		cv::findContours(thresh2, contourPoints2, hierarchy2, cv::RETR_TREE, cv::CHAIN_APPROX_TC89_KCOS);
		collected2.push_back(contourPoints2);
		contourPoints2.clear();
	}

	draw_contour_map(collected2, hierarchy2, cmap2, thresh2.cols, thresh2.rows, thresh2.type());

	allContours1 = cmap1.clone();
	allContours2 = cmap2.clone();

	dst1.clear();
	dst2.clear();
	size_t minC = std::min(collected1.size(), collected2.size());
	dst1.resize(minC);
	dst2.resize(minC);

	for (size_t i = 0; i < minC; ++i) {
		Mat &cont1 = dst1[i];
		Mat &cont2 = dst2[i];
		cont1 = Mat::zeros(img1.rows, img1.cols, img1.type());
		cont2 = Mat::zeros(img2.rows, img2.cols, img2.type());
		contourPoints1 = collected1[i];
		contourPoints2 = collected2[i];

		for (size_t j = 0; j < contourPoints1.size(); ++j) {
			cv::drawContours(cont1, contourPoints1, j, { 255, 255, 255 }, 1.0, cv::LINE_8, hierarchy1, 0);
		}

		for (size_t j = 0; j < contourPoints2.size(); ++j) {
			cv::drawContours(cont2, contourPoints2, j, { 255, 255, 255 }, 1.0, cv::LINE_8, hierarchy2, 0);
		}

		cvtColor(cont1, cont1, cv::COLOR_RGB2GRAY);
		cvtColor(cont2, cont2, cv::COLOR_RGB2GRAY);
	}

	show_image("Conto1", allContours1);
	show_image("Conto2", allContours2);
}

void correct_rotation_and_position(const Mat &src1, const Mat &src2, Mat &dst1, Mat &dst2, vector<vector<vector<Point>>>& collected1, vector<vector<vector<Point>>>& collected2) {
	vector<Point> flat1;
	vector<Point> flat2;
	for(auto& c : collected1) {
		for(auto& v : c) {
			if(c.size() > 1) {
				RotatedRect rr1 = minAreaRect(v);
				if(fabs(rr1.center.x - ((src1.cols - 1.0) / 2.0)) < 0.01) {
					continue;
				}
			}
			for(auto& pt : v) {
				flat1.push_back(pt);
			}
		}
	}

	if(flat1.empty())
		flat1 = collected1[0][0];

	for(auto& c : collected2) {
		for(auto& v : c) {
			if(c.size() > 1) {
				RotatedRect rv2 = minAreaRect(v);
				if(fabs(rv2.center.x - ((src2.cols - 1.0) / 2.0)) < 0.01) {
					continue;
				}
			}
			for(auto& pt : v) {
				flat2.push_back(pt);
			}
		}
	}

	if(flat2.empty())
		flat2 = collected2[0][0];

	RotatedRect rr1 = minAreaRect(flat1);
	RotatedRect rr2 = minAreaRect(flat2);
    double targetAng = rr2.angle - rr1.angle;
    double wdiff = std::fabs(rr1.size.width - rr2.size.width);
    double hdiff = std::fabs(rr1.size.height - rr2.size.height);
    double wdiffrot = std::fabs(rr1.size.width - rr2.size.height);
    double hdiffrot = std::fabs(rr1.size.height - rr2.size.width);
    cerr << (wdiffrot + hdiffrot / 2.0) << " < " <<  (wdiff + hdiff / 2.0) << endl;
    if((wdiffrot + hdiffrot / 2.0) < (wdiff + hdiff / 2.0)) {
    	if(targetAng > 0)
    		targetAng=-90;
    	else if(targetAng < 0)
    		targetAng=+90;
    }

	if(targetAng > 0) {
		if(targetAng >= 270) {
			targetAng-=270;
		} else if(targetAng >= 180) {
			targetAng-=180;
		} else if(targetAng >= 90) {
			targetAng-=90;
		}
	} else if(targetAng < 0) {
		if(targetAng <= -270) {
			targetAng+=270;
		} else if(targetAng <= -180) {
			targetAng+=180;
		} else if(targetAng <= -90) {
			targetAng+=90;
		}
	}
	cv::Mat rm2 = getRotationMatrix2D(rr2.center, targetAng, 1.0);
    Mat rotated2;
    warpAffine(src2, rotated2, rm2, src2.size());
    float warpValues[] = { 1.0, 0.0, rr1.center.x - rr2.center.x, 0.0, 1.0, rr1.center.y - rr2.center.y };
    Mat translation_matrix = Mat(2, 3, CV_32F, warpValues);
    warpAffine(rotated2, dst2, translation_matrix, src2.size());

    dst1 = src1.clone();
}

void find_matches(Mat &orig1, Mat &orig2, Mat& corrected1, Mat& corrected2, std::vector<cv::Point2f> &srcPoints1, std::vector<cv::Point2f> &srcPoints2, Mat &allContours1, Mat &allContours2) {
	std::vector<Mat> contours1, contours2;
	vector<vector<vector<Point>>> collected1;
	vector<vector<vector<Point>>> collected2;

	find_contours(orig1, orig2, contours1, contours2, allContours1, allContours2, collected1, collected2);
	if(Settings::instance().enable_auto_transform) {
		correct_rotation_and_position(orig1, orig2, corrected1, corrected2, collected1, collected2);
		contours1.clear();
		contours2.clear();
		collected1.clear();
		collected2.clear();
		find_contours(corrected1, corrected2, contours1, contours2, allContours1, allContours2, collected1, collected2);
	} else {
		corrected1 = orig1.clone();
		corrected2 = orig2.clone();
	}

	for (size_t i = 0; i < contours1.size(); ++i) {
		auto matches = find_matches(contours1[i], contours2[i]);
		srcPoints1.insert(srcPoints1.end(), matches.first.begin(), matches.first.end());
		srcPoints2.insert(srcPoints2.end(), matches.second.begin(), matches.second.end());
	}

	std::cerr << "contour points: " << srcPoints1.size() << std::endl;
}

std::tuple<double, double, double> calculate_sum_mean_and_sd(std::multimap<double, std::pair<Point2f, Point2f>> distanceMap) {
	size_t s = distanceMap.size();
	double sum = 0.0, mean, standardDeviation = 0.0;

	for (auto &p : distanceMap) {
		sum += p.first;
	}

	mean = sum / s;

	for (auto &p : distanceMap) {
		standardDeviation += pow(p.first - mean, 2);
	}

	return {sum, mean, sqrt(standardDeviation / s)};
}

void match_points_by_proximity(std::vector<cv::Point2f> &srcPoints1, std::vector<cv::Point2f> &srcPoints2, int cols, int rows) {
	std::multimap<double, std::pair<Point2f, Point2f>> distanceMap;

	Point2f nopoint(-1, -1);
	for (auto &pt1 : srcPoints1) {
		double dist = 0;
		double currentMinDist = std::numeric_limits<double>::max();

		Point2f *closest = &nopoint;
		for (auto &pt2 : srcPoints2) {
			if (pt2.x == -1 && pt2.y == -1)
				continue;

			dist = hypot(pt2.x - pt1.x, pt2.y - pt1.y);

			if (dist < currentMinDist) {
				currentMinDist = dist;
				closest = &pt2;
			}
		}

		if (closest->x == -1 && closest->y == -1)
			continue;

		dist = hypot(closest->x - pt1.x, closest->y - pt1.y);
		distanceMap.insert( { dist, { pt1, *closest } });
		closest->x = -1;
		closest->y = -1;
	}
	auto distribution = calculate_sum_mean_and_sd(distanceMap);
	srcPoints1.clear();
	srcPoints2.clear();
	assert(!distanceMap.empty());
	assert(std::get<1>(distribution) != 0 && std::get<2>(distribution) != 0);
	double distance = (*distanceMap.rbegin()).first;
	double mean = std::get<1>(distribution);
	double sd = std::get<2>(distribution);
	double highZScore = (std::fabs(distance - mean) / sd) / (std::max(sd, mean) - std::fabs(sd - mean));
	assert(highZScore > 0);
	double zScore = 0;
	double value = 0;
	double limit = 0.1 * Settings::instance().match_tolerance * highZScore * std::fabs(sd - mean);

	for (auto it = distanceMap.rbegin(); it != distanceMap.rend(); ++it) {
		value = (*it).first;
		zScore = (mean / sd) - std::fabs((value - mean) / sd);
//		std::cerr
//				<< "\tm/s/l/z/h: " << mean << "/" << sd << "/" << limit << "/" << zScore << "/" << highZScore
//				<< std::endl;

		if (value < mean && zScore < limit) {
			srcPoints1.push_back((*it).second.first);
			srcPoints2.push_back((*it).second.second);
		}
	}

	assert(srcPoints1.size() == srcPoints2.size());
	check_points(srcPoints1, cols, rows);
	check_points(srcPoints2, cols, rows);
}

void add_corners(std::vector<cv::Point2f> &srcPoints1, std::vector<cv::Point2f> &srcPoints2, MatSize sz) {
	float w = sz().width - 1;
	float h = sz().height - 1;
	srcPoints1.push_back(cv::Point2f(0, 0));
	srcPoints1.push_back(cv::Point2f(w, 0));
	srcPoints1.push_back(cv::Point2f(0, h));
	srcPoints1.push_back(cv::Point2f(w, h));
	srcPoints2.push_back(cv::Point2f(0, 0));
	srcPoints2.push_back(cv::Point2f(w, 0));
	srcPoints2.push_back(cv::Point2f(0, h));
	srcPoints2.push_back(cv::Point2f(w, h));
}

void prepare_matches(Mat &src1, Mat &src2, const Mat &img1, const Mat &img2, vector<Point2f> &srcPoints1, vector<Point2f> &srcPoints2) {
	//edit matches
	std::cerr << "prepare: " << srcPoints1.size() << " -> ";
	match_points_by_proximity(srcPoints1, srcPoints2, img1.cols, img1.rows);
	std::cerr << "match: " << srcPoints1.size() << " -> ";

	Mat matMatches;
	Mat grey1, grey2;
	cvtColor(src1, grey1, cv::COLOR_RGB2GRAY);
	cvtColor(src2, grey2, cv::COLOR_RGB2GRAY);
	draw_matches(grey1, grey2, matMatches, srcPoints1, srcPoints2);
	show_image("matches reduced", matMatches);

	if (srcPoints1.size() > srcPoints2.size())
		srcPoints1.resize(srcPoints2.size());
	else
		srcPoints2.resize(srcPoints1.size());

	add_corners(srcPoints1, srcPoints2, src1.size);
}

double morph_images(const Mat &origImg1, const Mat &origImg2, cv::Mat &dst, const cv::Mat &last, std::vector<cv::Point2f> &morphedPoints, std::vector<cv::Point2f> srcPoints1, std::vector<cv::Point2f> srcPoints2, Mat &allContours1, Mat &allContours2, double shapeRatio, double maskRatio) {
	cv::Size SourceImgSize(origImg1.cols, origImg1.rows);
	cv::Subdiv2D subDiv1(cv::Rect(0, 0, SourceImgSize.width, SourceImgSize.height));
	cv::Subdiv2D subDiv2(cv::Rect(0, 0, SourceImgSize.width, SourceImgSize.height));
	cv::Subdiv2D subDivMorph(cv::Rect(0, 0, SourceImgSize.width, SourceImgSize.height));

	std::vector<cv::Point2f> uniq1, uniq2, uniqMorph;
	check_points(srcPoints1, origImg1.cols, origImg1.rows);
	make_uniq(srcPoints1, uniq1);
	check_uniq(uniq1);
	for (auto pt : uniq1)
		subDiv1.insert(pt);

	check_points(srcPoints2, origImg2.cols, origImg2.rows);
	make_uniq(srcPoints2, uniq2);
	check_uniq(uniq2);
	for (auto pt : uniq2)
		subDiv2.insert(pt);

	morph_points(srcPoints1, srcPoints2, morphedPoints, shapeRatio);
	assert(srcPoints1.size() == srcPoints2.size() && srcPoints2.size() == morphedPoints.size());

	check_points(morphedPoints, origImg1.cols, origImg1.rows);
	make_uniq(morphedPoints, uniqMorph);
	check_uniq(uniqMorph);
	for (auto pt : uniqMorph)
		subDivMorph.insert(pt);

	// Get the ID list of corners of Delaunay traiangles.
	std::vector<cv::Vec3i> triangleIndices;
	get_triangle_indices(subDivMorph, morphedPoints, triangleIndices);

	// Get coordinates of Delaunay corners from ID list
	std::vector<std::vector<cv::Point2f>> triangleSrc1, triangleSrc2, triangleMorph;
	make_triangler_points(triangleIndices, srcPoints1, triangleSrc1);
	make_triangler_points(triangleIndices, srcPoints2, triangleSrc2);
	make_triangler_points(triangleIndices, morphedPoints, triangleMorph);

	// Create a map of triangle ID in the morphed image.
	cv::Mat triMap = cv::Mat::zeros(SourceImgSize, CV_32SC1);
	paint_triangles(triMap, triangleMorph);

	// Compute Homography matrix of each triangle.
	std::vector<cv::Mat> homographyMats, morphHom1, morphHom2;
	solve_homography(triangleSrc1, triangleSrc2, homographyMats);
	morph_homography(homographyMats, morphHom1, morphHom2, shapeRatio);

	cv::Mat trImg1;
	cv::Mat trans_map_x1, trans_map_y1;
	create_map(triMap, morphHom1, trans_map_x1, trans_map_y1);
	cv::remap(origImg1, trImg1, trans_map_x1, trans_map_y1, cv::INTER_LINEAR);

	cv::Mat trImg2;
	cv::Mat trMapX2, trMapY2;
	create_map(triMap, morphHom2, trMapX2, trMapY2);
	cv::remap(origImg2, trImg2, trMapX2, trMapY2, cv::INTER_LINEAR);

	Mat_<Vec3f> l;
	Mat_<Vec3f> r;
	trImg1.convertTo(l, CV_32F, 1.0 / 255.0);
	trImg2.convertTo(r, CV_32F, 1.0 / 255.0);
	Mat_<float> m(l.rows, l.cols, 0.0);
	Mat_<float> m1(l.rows, l.cols, 0.0);
	Mat_<float> m2(l.rows, l.cols, 0.0);
	equalizeHist(allContours1, allContours1);
	equalizeHist(allContours1, allContours1);

	for (off_t x = 0; x < m1.cols; ++x) {
		for (off_t y = 0; y < m1.rows; ++y) {
			m2.at<float>(y, x) = allContours1.at<uint8_t>(y, x) / 255.0;
		}
	}

	for (off_t x = 0; x < m2.cols; ++x) {
		for (off_t y = 0; y < m2.rows; ++y) {
			m1.at<float>(y, x) = allContours2.at<uint8_t>(y, x) / 255.0;
		}
	}

	m2(Range::all(), Range::all()) = 1.0;
	Mat mask;
	m = (m2 * (1.0 - maskRatio) + m1 * maskRatio);

	off_t kx = std::round(m.cols / 32.0);
	off_t ky = std::round(m.rows / 32.0);
	if (kx % 2 != 1)
		kx -= 1;

	if (ky % 2 != 1)
		ky -= 1;

	GaussianBlur(m, mask, Size(kx, ky), 12);
	LaplacianBlending lb(l, r, mask, Settings::instance().pyramid_levels);
	Mat_<Vec3f> lapBlend = lb.blend().clone();
	lapBlend.convertTo(dst, origImg1.depth(), 255.0);
	Mat analysis = dst.clone();
	Mat prev = last.clone();
	if (prev.empty())
		prev = dst.clone();
	draw_morph_analysis(dst, prev, analysis, SourceImgSize, subDiv1, subDiv2, subDivMorph, { 0, 0, 255 });
	show_image("analysis", analysis);
	return 0;
}
}