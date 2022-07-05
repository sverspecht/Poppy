#include "util.hpp"
#include "settings.hpp"

#include <set>
#include <iostream>

#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

using namespace std;
using namespace cv;

namespace poppy {

double feature_metric(const Mat &grey1) {
	Mat corners;
	cornerHarris(grey1, corners, 2, 3, 0.04);
	cv::Scalar mean, stddev;
	cv::meanStdDev(corners, mean, stddev);

	return stddev[0];
}

pair<double, Point2f> get_orientation(const vector<Point2f> &pts)
		{
	//Construct a buffer used by the pca analysis
	int sz = static_cast<int>(pts.size());
	Mat data_pts = Mat(sz, 2, CV_64F);
	for (int i = 0; i < data_pts.rows; i++)	{
		data_pts.at<double>(i, 0) = pts[i].x;
		data_pts.at<double>(i, 1) = pts[i].y;
	}
	//Perform PCA analysis
	PCA pca_analysis(data_pts, Mat(), PCA::DATA_AS_ROW);
	//Store the center of the object
	Point2f cntr = Point2f(pca_analysis.mean.at<double>(0, 0), pca_analysis.mean.at<double>(0, 1));
	//Store the eigenvalues and eigenvectors
	vector<Point2d> eigen_vecs(2);
	vector<double> eigen_val(2);
	for (int i = 0; i < 2; i++) {
		eigen_vecs[i] = Point2d(pca_analysis.eigenvectors.at<double>(i, 0), pca_analysis.eigenvectors.at<double>(i, 1));
		eigen_val[i] = pca_analysis.eigenvalues.at<double>(i);
	}

	return {atan2(eigen_vecs[0].y, eigen_vecs[0].x) * RADIANS_TO_DEGREES, cntr};
}

std::tuple<double, double, double> calculate_sum_mean_and_sd(multimap<double, pair<Point2f, Point2f>> distanceMap) {
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

void add_corners(vector<Point2f> &srcPoints1, vector<Point2f> &srcPoints2, MatSize sz) {
	float w = sz().width - 1;
	float h = sz().height - 1;
	srcPoints1.push_back(Point2f(0, 0));
	srcPoints1.push_back(Point2f(w, 0));
	srcPoints1.push_back(Point2f(0, h));
	srcPoints1.push_back(Point2f(w, h));
	srcPoints2.push_back(Point2f(0, 0));
	srcPoints2.push_back(Point2f(w, 0));
	srcPoints2.push_back(Point2f(0, h));
	srcPoints2.push_back(Point2f(w, h));
}

void adjust_contrast_and_brightness(const Mat &src, Mat &dst, double contrast, double lowcut) {
	dst = src.clone();
	int minV = numeric_limits<int>::max();
	int maxV = numeric_limits<int>::min();
	double val = 0;
	for (int y = 0; y < src.rows; y++) {
		for (int x = 0; x < src.cols; x++) {
			val = double(dst.at<uchar>(y, x));
			maxV = max(double(maxV), val);
			minV = min(double(minV), val);
		}
	}

	for (int y = 0; y < src.rows; y++) {
		for (int x = 0; x < src.cols; x++) {
			val = dst.at<uchar>(y, x);
			dst.at<uchar>(y, x) = ((val - minV) / (maxV - minV)) * 255;
		}
	}

	Mat hc(dst);
	Scalar imgAvgVec = sum(dst) / (dst.cols * dst.rows);
	double imgAvg = imgAvgVec[0];
	int brightness = -((contrast - 1) * imgAvg);
	dst.convertTo(hc, -1, contrast, brightness);
	hc.copyTo(dst);
}


std::vector<std::vector<Point2f>> convertContourTo2f(const std::vector<std::vector<Point>> &contours1) {
	std::vector<std::vector<Point2f>> tmp;
	for (auto &vec : contours1) {
		std::vector<Point2f> row;
		for (auto &pt : vec) {
			row.push_back(Point2f(pt.x, pt.y));
		}
		if (!row.empty())
			tmp.push_back(row);
	}
	return tmp;
}

std::vector<std::vector<Point>> convertContourFrom2f(const std::vector<std::vector<Point2f>> &contours1) {
	std::vector<std::vector<Point>> tmp;
	for (auto &vec : contours1) {
		std::vector<Point> row;
		for (auto &pt : vec) {
			row.push_back(Point(pt.x, pt.y));
		}
		if (!row.empty())
			tmp.push_back(row);
	}
	return tmp;
}
void overdefineHull(vector<Point2f>& hull, size_t minPoints) {
	assert(hull.size() > 1);
	off_t diff = minPoints - hull.size();
	while(diff > 0)  {
		for(size_t i = 0; i < hull.size() && diff > 0; i+=2) {
			const auto& first = hull[i];
			const auto& second = hull[(i + 1) % hull.size()];
			auto insertee = first;
			auto vector = second - first;
			vector.x /= 2.0;
			vector.y /= 2.0;
			insertee.x += vector.x;
			insertee.y += vector.y;
			hull.insert(hull.begin() + (i+1), std::move(insertee));
			--diff;
		}
	}
}

double morph_distance(const vector<Point2f>& srcPoints1, const vector<Point2f>& srcPoints2, const double& width, const double& height) {
	assert(srcPoints1.size() == srcPoints2.size());
	double totalDistance = 0;
	for(size_t i = 0; i < srcPoints1.size(); ++i) {
		Point2f v = srcPoints2[i] - srcPoints1[i];
		totalDistance += hypot(v.x, v.y);
	}
	return (totalDistance / srcPoints1.size()) / hypot(width,height) * 1000.0;
}

void show_image(const string &name, const Mat &img) {
#ifndef _WASM
	if(Settings::instance().show_gui) {
		namedWindow(name, WINDOW_NORMAL | WINDOW_KEEPRATIO | WINDOW_GUI_EXPANDED);
		imshow(name, img);
	}
#endif
}

void wait_key(int timeout) {
#ifndef _WASM
	if(Settings::instance().show_gui) {
		waitKey(timeout);
	}
#endif
}

void clip_points(std::vector<Point2f> &pts, int cols, int rows) {
	for (auto &pt : pts) {
		pt.x = pt.x > cols ? cols - 1 : pt.x;
		pt.y = pt.y > rows ? rows - 1 : pt.y;
		pt.x = pt.x < 0 ? 0 : pt.x;
		pt.y = pt.y < 0 ? 0 : pt.y;
	}
}

void check_points(const std::vector<Point2f> &pts, int cols, int rows) {
#ifndef NDEBUG
	for (const auto &pt : pts) {
		assert(!isinf(pt.x) && !isinf(pt.y));
		assert(!isnan(pt.x) && !isnan(pt.y));
		assert(pt.x >= 0 && pt.y >= 0);
		assert(pt.x <= cols && pt.y <= rows);
	}
#endif
}

void filter_invalid_points(vector<Point2f>& srcPoints1, vector<Point2f>& srcPoints2, int cols, int rows) {
	for (size_t i = 0; i < srcPoints1.size(); ++i) {
		Point2f& pt = srcPoints1[i];
		if(pt.x < 0 || pt.x > cols || pt.y < 0 || pt.y > rows) {
			srcPoints1.erase(srcPoints1.begin()+i);
			srcPoints2.erase(srcPoints2.begin()+i);
			--i;
		}
	}
	for (size_t i = 0; i < srcPoints2.size(); ++i) {
		Point2f& pt = srcPoints2[i];
		if(pt.x < 0 || pt.x > cols || pt.y < 0 || pt.y > rows) {
			srcPoints1.erase(srcPoints1.begin()+i);
			srcPoints2.erase(srcPoints2.begin()+i);
			--i;
		}
	}
}


void check_uniq(const std::vector<Point2f> &pts) {
#ifndef NDEBUG
	std::set<Point2f, LessPointOp> uniq;
	for (const auto &pt : pts) {
		assert(uniq.insert(pt).second);
	}
#endif
}

void check_min_distance(const std::vector<Point2f> &in, double minDistance) {
#ifndef NDEBUG
	double dist = 0;
	for (size_t i = 0; i < in.size(); ++i) {
		const auto& pt1 = in[i];
		for (size_t j = 0; j < in.size(); ++j) {
			if(i == j)
				continue;
			const auto& pt2 = in[j];
			dist = hypot(pt2.x - pt1.x, pt2.y - pt1.y);
			assert(dist >= minDistance);
		}
	}
#endif
}

void filter_min_distance(const std::vector<Point2f> &in, std::vector<Point2f> &out, double minDistance) {
	double dist = 0;
	for (size_t i = 0; i < in.size(); ++i) {
		const auto& pt1 = in[i];
		out.push_back(pt1);
		for (size_t j = i + 1; j < in.size(); ++j) {
			assert(i != j);
			const auto& pt2 = in[j];
			dist = hypot(pt2.x - pt1.x, pt2.y - pt1.y);
			if(dist >= minDistance) {
				out.push_back(pt2);
			}
		}
	}
}

void make_uniq(const std::vector<Point2f> &pts, std::vector<Point2f> &out) {
	std::set<Point2f, LessPointOp> uniq;
	for (const auto &pt : pts) {
		if (uniq.insert(pt).second) {
			out.push_back(pt);
		}
	}
}

bool operator==(const Point2f &pt1, const Point2f &pt2) {
	return pt1.x == pt2.x && pt1.y == pt2.y;
}

bool operator==(const KeyPoint &kp1, const KeyPoint &kp2) {
	return kp1.pt.x == kp2.pt.x && kp1.pt.y == kp2.pt.y;
}

double distance(const Point2f &p1, const Point2f &p2) {
	return hypot(p2.x - p1.x, p2.y - p1.y);
}

cv::Point2f average(const std::vector<cv::Point2f> &pts) {
	Point2f result;
	for(const auto& pt : pts) {
		result += pt;
	}

	result.x /= pts.size();
	result.y /= pts.size();

	return result;
}
}
