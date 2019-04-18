//
// Copyright(c) Multimedia Signal Processing Group (MMSPG),
//              Ecole Polytechnique Fédérale de Lausanne (EPFL)
//              http://mmspg.epfl.ch
// All rights reserved.
// Author: Philippe Hanhart (philippe.hanhart@epfl.ch)
//
// Permission is hereby granted, without written agreement and without
// license or royalty fees, to use, copy, modify, and distribute the
// software provided and its documentation for research purpose only,
// provided that this copyright notice and the original authors' names
// appear on all copies and supporting documentation.
// The software provided may not be commercially distributed.
// In no event shall the Ecole Polytechnique Fédérale de Lausanne (EPFL)
// be liable to any party for direct, indirect, special, incidental, or
// consequential damages arising out of the use of the software and its
// documentation.
// The Ecole Polytechnique Fédérale de Lausanne (EPFL) specifically
// disclaims any warranties.
// The software provided hereunder is on an "as is" basis and the Ecole
// Polytechnique Fédérale de Lausanne (EPFL) has no obligation to provide
// maintenance, support, updates, enhancements, or modifications.
//

/**************************************************************************

 Usage:
  VQMT.exe OriginalVideo ProcessedVideo Height Width NumberOfFrames ChromaFormat Output Metrics

  OriginalVideo: the original video as raw YUV video file, progressively scanned, and 8 bits per sample
  ProcessedVideo: the processed video as raw YUV video file, progressively scanned, and 8 bits per sample
  Height: the height of the video
  Width: the width of the video
  NumberOfFrames: the number of frames to process
  ChromaFormat: the chroma subsampling format. 0: YUV400, 1: YUV420, 2: YUV422, 3: YUV444
  Output: the name of the output file(s)
  Metrics: the list of metrics to use
   available metrics:
   - PSNR: Peak Signal-to-Noise Ratio (PSNR)
   - SSIM: Structural Similarity (SSIM)
   - MSSSIM: Multi-Scale Structural Similarity (MS-SSIM)
   - VIFP: Visual Information Fidelity, pixel domain version (VIFp)
   - PSNRHVS: Peak Signal-to-Noise Ratio taking into account Contrast Sensitivity Function (CSF) (PSNR-HVS)
   - PSNRHVSM: Peak Signal-to-Noise Ratio taking into account Contrast Sensitivity Function (CSF) and between-coefficient contrast masking of DCT basis functions (PSNR-HVS-M)

And also Spherical metrics:
   - WSPSNR: Weighted-to-spherical PSNR

 Example:
  VQMT.exe original.yuv processed.yuv 1088 1920 250 1 results PSNR SSIM MSSSIM VIFP
  will create the following output files in CSV (comma-separated values) format:
  - results_pnsr.csv
  - results_ssim.csv
  - results_msssim.csv
  - results_vifp.csv

 Notes:
 - SSIM comes for free when MSSSIM is computed (but you still need to specify it to get the output)
 - PSNRHVS and PSNRHVSM are always computed at the same time (but you still need to specify both to get the two outputs)
 - When using MSSSIM, the height and width of the video have to be multiple of 16
 - When using VIFP, the height and width of the video have to be multiple of 8

 Changes in version 1.1 (since 1.0) on 30/3/13
 - Added support for large files (>2GB)
 - Added support for different chroma sampling formats (YUV400, YUV420, YUV422, and YUV444)

**************************************************************************/

#include <iostream>
#include <stdio.h>
#include <string.h>
#include <string>
#include <opencv2/core/core.hpp>

//Boost::program_options
#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include "VideoYUV.hpp"
#include "PSNR.hpp"
#include "SSIM.hpp"
#include "MSSSIM.hpp"
#include "VIFP.hpp"
#include "PSNRHVS.hpp"

// Spherical metrics
#include "WSPSNR.hpp"

enum Metrics {
    METRIC_PSNR = 0,
    METRIC_SSIM,
    METRIC_MSSSIM,
    METRIC_VIFP,
    METRIC_PSNRHVS,
    METRIC_PSNRHVSM,

    METRIC_WSPSNR,
    METRIC_WSSSIM,
    METRIC_WSMMSSSIM,
    METRIC_SIZE
};

std::map<std::string, Metrics> metric2index = {
    {"PSNR", METRIC_PSNR},
    {"SSIM", METRIC_SSIM},
    {"MSSSIM", METRIC_MSSSIM},
    {"VIFP", METRIC_VIFP},
    {"PSNRHVS", METRIC_PSNRHVS},
    {"PSNRHVSM", METRIC_PSNRHVSM},
    {"WSPSNR", METRIC_WSPSNR},
};

int main (int argc, const char *argv[])
{
    po::options_description desc("Allowed options");
    desc.add_options()
      ("help",          "produce this help message")
      ("original,i",    po::value<std::string>(), "Original video stream (YUV)")
      ("processed,p",   po::value<std::string>(), "Processed video stream (YUV)")
      ("width,w",       po::value<int>(), "Width")
      ("height,h",      po::value<int>(), "Height")
      ("frames,f",      po::value<int>(), "Number of frames")
      ("chroma,c",      po::value<int>(), "Chroma format")
      ("results,r",     po::value<std::string>(), "Output dir for results")
      ("metrics,m",     po::value<std::vector<std::string>>()->multitoken(), "Metrics to compute")
      ;

    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv).options(desc).run(), vm);
    po::notify(vm);

    if (vm.count("help")) {
        std::cout << desc << "\n";
        return 1;
    }

    double duration = static_cast<double>(cv::getTickCount());

    // Input parameters.
    int width    = vm["width"].as<int>();
    int height   = vm["height"].as<int>();
    int nbframes = vm["frames"].as<int>();
    int chroma   = vm["chroma"].as<int>();

    std::string orig_path = vm["original"].as<std::string>();
    std::string proc_path = vm["processed"].as<std::string>();
    std::string results_path = vm["processed"].as<std::string>();

    // Input video streams.
    VideoYUV *original  = new VideoYUV(orig_path.c_str(), height, width, nbframes, chroma);
    VideoYUV *processed = new VideoYUV(proc_path.c_str(), height, width, nbframes, chroma);

    // Output files for results.
    FILE *result_file[METRIC_SIZE] = {nullptr};
    char *str = new char[256];
    for (auto metric : vm["metrics"].as<std::vector<std::string>>()) {
        if (metric2index.count (metric)) {
            sprintf(str, "%s_%s.csv", results_path.c_str(), metric.c_str ());
            result_file[metric2index[metric]] = fopen(str, "w");
        }
        else {
            printf ("Warning: Metric %s not recognized and will be ignored.\n", metric.c_str() );
        }
    }
    delete[] str;

    // Check size for VIFp downsampling.
    if (result_file[METRIC_VIFP] != nullptr && (height % 8 != 0 || width % 8 != 0)) {
        fprintf(stderr, "VIFp: 'height' and 'width' have to be multiple of 8.\n");
        exit(EXIT_FAILURE);
    }

    // Check size for MS-SSIM downsampling.
    if (result_file[METRIC_MSSSIM] != nullptr && (height % 16 != 0 || width % 16 != 0)) {
        fprintf(stderr, "MS-SSIM: 'height' and 'width' have to be multiple of 16.\n");
        exit(EXIT_FAILURE);
    }

    // Print header to file.
    for (int m=0; m<METRIC_SIZE; m++) {
        if (result_file[m] != nullptr) {
            fprintf(result_file[m], "frame,value\n");
        }
    }

    PSNR *psnr     = new PSNR(height, width);
    SSIM *ssim     = new SSIM(height, width);
    MSSSIM *msssim = new MSSSIM(height, width);
    VIFP *vifp     = new VIFP(height, width);
    PSNRHVS *phvs  = new PSNRHVS(height, width);

    // Spherical metrics.
    WSPSNR *wspsnr = new WSPSNR(height, width);

    cv::Mat original_frame(height,width,CV_32F), processed_frame(height,width,CV_32F);
    float result[METRIC_SIZE] = {0};
    float result_avg[METRIC_SIZE] = {0};

    for (int frame=0; frame<nbframes; frame++) {
        printf ("Computing metrics for frame %d.\n", frame);

        if (!original->readOneFrame()) exit(EXIT_FAILURE);
        original->getLuma(original_frame, CV_32F);

        if (!processed->readOneFrame()) exit(EXIT_FAILURE);
        processed->getLuma(processed_frame, CV_32F);

        // Compute PSNR
        if (result_file[METRIC_PSNR] != nullptr) {
            result[METRIC_PSNR] = psnr->compute(original_frame, processed_frame);
        }

        // Compute SSIM and MS-SSIM
        if (result_file[METRIC_SSIM] != nullptr && result_file[METRIC_MSSSIM] == nullptr) {
            result[METRIC_SSIM] = ssim->compute(original_frame, processed_frame);
        }

        if (result_file[METRIC_MSSSIM] != nullptr) {
            msssim->compute(original_frame, processed_frame);

            if (result_file[METRIC_SSIM] != nullptr) {
                result[METRIC_SSIM] = msssim->getSSIM();
            }

            result[METRIC_MSSSIM] = msssim->getMSSSIM();
        }

        // Compute VIFp,
        if (result_file[METRIC_VIFP] != nullptr) {
            result[METRIC_VIFP] = vifp->compute(original_frame, processed_frame);
        }

        // Compute PSNR-HVS and PSNR-HVS-M,
        if (result_file[METRIC_PSNRHVS] != nullptr || result_file[METRIC_PSNRHVSM] != nullptr) {
            phvs->compute(original_frame, processed_frame);

            if (result_file[METRIC_PSNRHVS] != nullptr) {
                result[METRIC_PSNRHVS] = phvs->getPSNRHVS();
            }

            if (result_file[METRIC_PSNRHVSM] != nullptr) {
                result[METRIC_PSNRHVSM] = phvs->getPSNRHVSM();
            }
        }

        // Compute WSPSNR,
        if (result_file[METRIC_WSPSNR] != nullptr) {
            result[METRIC_WSPSNR] = wspsnr->compute(original_frame, processed_frame);
        }

        printf ( "PSNR: %.3f, WSPSNR: %.3f\n",
                 static_cast<double>(result[METRIC_PSNR]),
                 static_cast<double>(result[METRIC_WSPSNR]) );

        // Print quality index to file
        for (int m=0; m<METRIC_SIZE; m++) {
            if (result_file[m] != nullptr) {
                result_avg[m] += result[m];
                fprintf(result_file[m], "%d,%.6f\n", frame, static_cast<double>(result[m]));
            }
        }
    }

    // Print average quality index to file
    for (int m = 0; m < METRIC_SIZE; m++) {
        if (result_file[m] != nullptr) {
            result_avg[m] /= static_cast<float>(nbframes);
            fprintf(result_file[m], "average,%.6f", static_cast<double>(result_avg[m]));
            fclose(result_file[m]);
        }
    }

    delete psnr;
    delete ssim;
    delete msssim;
    delete vifp;
    delete phvs;

    delete wspsnr;

    delete original;
    delete processed;

    duration = static_cast<double>(cv::getTickCount()) - duration;
    duration /= cv::getTickFrequency();
    printf("Time: %0.3fs\n", duration);

    return EXIT_SUCCESS;
}

