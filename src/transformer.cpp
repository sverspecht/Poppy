#include "transformer.hpp"
#include "draw.hpp"

#include <limits>
#include <iostream>

using std::cerr;
using std::cout;
using std::endl;

namespace poppy {

Transformer::Transformer(const size_t &width, const size_t &height) :
		width_(width), height_(height) {
}

Transformer::~Transformer() {
}

void Transformer::translate(const Mat &src, Mat &dst, const Point2f &by) {
	float warpValues[] = { 1.0, 0.0, by.x, 0.0, 1.0, by.y };
	Mat translation_matrix = Mat(2, 3, CV_32F, warpValues);
	warpAffine(src, dst, translation_matrix, src.size(), INTER_LINEAR, BORDER_CONSTANT, Scalar(0, 0, 0));
}

void Transformer::rotate(const Mat &src, Mat &dst, Point2f center, double angle, double scale) {
	Mat rm = getRotationMatrix2D(center, angle, scale);
	warpAffine(src, dst, rm, src.size());
}

Point2f Transformer::rotate_point(const cv::Point2f &inPoint, const double &angDeg) {
	double rad = angDeg * M_PI / 180.0;
	cv::Point2f outPoint;
	outPoint.x = std::cos(rad) * inPoint.x - std::sin(rad) * inPoint.y;
	outPoint.y = std::sin(rad) * inPoint.x + std::cos(rad) * inPoint.y;
	return outPoint;
}

Point2f Transformer::rotate_point(const cv::Point2f &inPoint, const cv::Point2f &center, const double &angDeg) {
	return rotate_point(inPoint - center, angDeg) + center;
}

void Transformer::translate_points(vector<Point2f> &pts, const Point2f &by) {
	for (auto &pt : pts) {
		pt += by;
	}
}

void Transformer::rotate_points(vector<Point2f> &pts, const Point2f &center, const double &angDeg) {
	for (auto &pt : pts) {
		pt = rotate_point(pt - center, angDeg) + center;
	}
}

void Transformer::scale_points(vector<Point2f> &pts, double coef) {
	for (auto &pt : pts) {
		pt.x *= coef;
		pt.y *= coef;
	}
}

void Transformer::translate_features(Features &ft, const Point2f &by) {
	translate_points(ft.chin_, by);
	translate_points(ft.top_nose_, by);
	translate_points(ft.bottom_nose_, by);
	translate_points(ft.left_eyebrow_, by);
	translate_points(ft.right_eyebrow_, by);
	translate_points(ft.left_eye_, by);
	translate_points(ft.right_eye_, by);
	translate_points(ft.outer_lips_, by);
	translate_points(ft.inside_lips_, by);
}

void Transformer::scale_features(Features &ft, double coef) {
	scale_points(ft.chin_, coef);
	scale_points(ft.top_nose_, coef);
	scale_points(ft.bottom_nose_, coef);
	scale_points(ft.left_eyebrow_, coef);
	scale_points(ft.right_eyebrow_, coef);
	scale_points(ft.left_eye_, coef);
	scale_points(ft.right_eye_, coef);
	scale_points(ft.outer_lips_, coef);
	scale_points(ft.inside_lips_, coef);
}

void Transformer::rotate_features(Features &ft, const cv::Point2f &center, const double &angDeg) {
	rotate_points(ft.chin_, center, angDeg);
	rotate_points(ft.top_nose_, center, angDeg);
	rotate_points(ft.bottom_nose_, center, angDeg);
	rotate_points(ft.left_eyebrow_, center, angDeg);
	rotate_points(ft.right_eyebrow_, center, angDeg);
	rotate_points(ft.left_eye_, center, angDeg);
	rotate_points(ft.right_eye_, center, angDeg);
	rotate_points(ft.outer_lips_, center, angDeg);
	rotate_points(ft.inside_lips_, center, angDeg);
}

double Transformer::retranslate(Mat &corrected2, vector<Point2f> &srcPointsFlann1, vector<Point2f> &srcPointsFlann2, vector<Point2f> &srcPointsRaw2) {
	vector<Point2f> left;
	vector<Point2f> right;
	vector<Point2f> top;
	vector<Point2f> bottom;
	for (auto &pt : srcPointsFlann2) {
		left.push_back( { pt.x - 1, pt.y });
		right.push_back( { pt.x + 1, pt.y });
		top.push_back( { pt.x, pt.y - 1 });
		bottom.push_back( { pt.x, pt.y + 1 });
	}
	double mdCurrent = morph_distance(srcPointsFlann1, srcPointsFlann2, width_, height_);
	double mdLeft = morph_distance(srcPointsFlann1, left, width_, height_);
	double mdRight = morph_distance(srcPointsFlann1, right, width_, height_);
	double mdTop = morph_distance(srcPointsFlann1, top, width_, height_);
	double mdBottom = morph_distance(srcPointsFlann1, bottom, width_, height_);
	off_t xchange = 0;
	off_t ychange = 0;

	if (mdLeft < mdCurrent)
		xchange = -1;
	else if (mdRight < mdCurrent)
		xchange = +1;

	if (mdTop < mdCurrent)
		ychange = -1;
	else if (mdBottom < mdCurrent)
		ychange = +1;
//	cerr << "current morph dist: " << mdCurrent << endl;
	off_t xProgress = 1;
	off_t yProgress = 1;

	if (xchange != 0 && ychange != 0) {
		double lastMorphDist = mdCurrent;
		double morphDist = 0;
		vector<Point2f> tmp = srcPointsFlann2;
		do {
			for (auto &pt : tmp) {
				pt.x += xchange;
				pt.y += ychange;
			}
			morphDist = morph_distance(srcPointsFlann1, tmp, width_, height_);
//			cerr << "morph dist x: " << morphDist << endl;
			if (morphDist > lastMorphDist)
				break;
			mdCurrent = lastMorphDist = morphDist;
			++xProgress;
			++yProgress;
		} while (true);
	} else {
		if (xchange != 0) {
			double lastMorphDist = mdCurrent;
			double morphDist = 0;
			vector<Point2f> tmp = srcPointsFlann2;
			do {
				for (auto &pt : tmp) {
					pt.x += xchange;
				}
				morphDist = morph_distance(srcPointsFlann1, tmp, width_, height_);
//			cerr << "morph dist x: " << morphDist << endl;
				if (morphDist > lastMorphDist)
					break;
				mdCurrent = lastMorphDist = morphDist;
				++xProgress;
			} while (true);
		}

		if (ychange != 0) {
			double lastMorphDist = mdCurrent;
			double morphDist = 0;
			vector<Point2f> tmp = srcPointsFlann2;
			do {
				for (auto &pt : tmp) {
					pt.y += ychange;
				}
				morphDist = morph_distance(srcPointsFlann1, tmp, width_, height_);
//			cerr << "morph dist y: " << morphDist << endl;
				if (morphDist > lastMorphDist)
					break;
				mdCurrent = lastMorphDist = morphDist;
				++yProgress;
			} while (true);
		}
	}
	Point2f retranslation(xchange * xProgress, ychange * yProgress);
//	cerr << "retranslation: " << mdCurrent << " " << retranslation << endl;
	translate(corrected2, corrected2, retranslation);
	for (auto &pt : srcPointsFlann2) {
		pt.x += retranslation.x;
		pt.y += retranslation.y;
	}

	for (auto &pt : srcPointsRaw2) {
		pt.x += retranslation.x;
		pt.y += retranslation.y;
	}

	return morph_distance(srcPointsFlann1, srcPointsFlann2, width_, height_);
}

double Transformer::rerotate(Mat &corrected2, vector<Point2f> &srcPointsFlann1, vector<Point2f> &srcPointsFlann2, vector<Point2f> &srcPointsRaw2) {
	double morphDist = -1;
	vector<Point2f> tmp;
	Point2f center = average(srcPointsFlann2);
	double lowestDist = std::numeric_limits<double>::max();
	double selectedAngle = 0;
	for (size_t i = 0; i < 3600; ++i) {
		tmp = srcPointsFlann2;
		rotate_points(tmp, center, i / 10.0);
		morphDist = morph_distance(srcPointsFlann1, tmp, width_, height_); //+ (pow(i / 3600.0, 2) * 3600);
		if (morphDist < lowestDist) {
			lowestDist = morphDist;
			selectedAngle = i / 10.0;
		}
	}

	rotate(corrected2, corrected2, center, -selectedAngle);
//	cerr << "rerotate: " << lowestDist << " selected angle: " << selectedAngle << "°" << endl;
	rotate_points(srcPointsFlann2, center, selectedAngle);
	rotate_points(srcPointsRaw2, center, selectedAngle);

	return morph_distance(srcPointsFlann1, srcPointsFlann2, width_, height_);
}

double Transformer::rescale(Mat &corrected2, vector<Point2f> &srcPointsFlann1, vector<Point2f> &srcPointsFlann2, vector<Point2f> &srcPointsRaw2) {
	double morphDist = -1;
	Point2f center = average(srcPointsFlann1);

	double lowestDist = std::numeric_limits<double>::max();
	Mat selectedMat;
	vector<Point2f> tmp;
	Mat m(3,3,CV_32F);
	double step = 1.0 / 100000;
	double scale = 0;
	for(size_t i = 0; i < 100; ++i) {
		scale = (step * (i + 1)) * ((corrected2.cols + corrected2.rows));
		m.at<float>(0,0) = scale;
		m.at<float>(1,0) = 0;
		m.at<float>(2,0) = 0;

		m.at<float>(0,1) = 0;
		m.at<float>(1,1) = scale;
		m.at<float>(2,1) = 0;

		m.at<float>(0,2) = center.x - (corrected2.cols * scale / 2.0);
		m.at<float>(1,2) = center.y - (corrected2.rows * scale / 2.0);
		m.at<float>(2,2) = 1.0;
		perspectiveTransform(srcPointsFlann2,tmp,m);
		morphDist = morph_distance3(srcPointsFlann1, tmp, width_, height_);
		if (morphDist < lowestDist) {
			lowestDist = morphDist;
			selectedMat = m.clone();
		}
	}

	if(!selectedMat.empty()) {
		perspectiveTransform(srcPointsRaw2,srcPointsRaw2,selectedMat);
		perspectiveTransform(srcPointsFlann2,srcPointsFlann2,selectedMat);
		selectedMat.pop_back();
		warpAffine(corrected2, corrected2, selectedMat, corrected2.size(), WARP_INVERSE_MAP);
	}

	return morph_distance(srcPointsFlann1, srcPointsFlann2, width_, height_);
}

} /* namespace poppy */
