#pragma once
#include <array>
#include <limits>
struct PreprocessingParameter {
    unsigned medianRadius = 1;
    double isotropicSpacing = 0;
};
struct SegmentationParameter {
    float lower = 1150, upper = std::numeric_limits<float>::max();
    bool largestComponent = false;
    unsigned closingRadius = 0;
};
struct RegistrationParameter {
    unsigned iterations = 40;
    double learningRate = 1;
};
struct ReconstructionParameter {
    double isoValue = 0.5;
};
struct SurfaceProcessingParameter {
    int smoothingIterations = 15;
    double reduction = 0;
    bool normals = true;
};
