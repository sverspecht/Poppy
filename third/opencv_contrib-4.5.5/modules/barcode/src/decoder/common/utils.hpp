// This file is part of OpenCV project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://opencv.org/license.html.
// Copyright (c) 2020-2021 darkliang wangberlinT Certseeds

#ifndef __OPENCV_BARCODE_UTILS_HPP__
#define __OPENCV_BARCODE_UTILS_HPP__


namespace cv {
namespace barcode {

enum BinaryType
{
    OTSU = 0, HYBRID = 1
};
static constexpr BinaryType binary_types[] = {OTSU, HYBRID};

void sharpen(const Mat &src, const Mat &dst);

void binarize(const Mat &src, Mat &dst, BinaryType mode);

}
}
#endif //__OPENCV_BARCODE_UTILS_HPP__
