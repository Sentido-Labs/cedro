    NVGpaint sky =
        nvgLinearGradient(vg, 0, 0, 0, horizon,
                       nvgHSLA(0.60, 1.00, 0.20*luma, 255),
                       nvgHSLA(0.60, 1.00, 0.90*luma, 255));
    NVGpaint ground =
        nvgLinearGradient(vg, 0, horizon, 0, fbHeight,
                       nvgHSLA(0.34, 0.73, 0.20*luma, 255),
                       nvgHSLA(0.34, 0.73, 0.67*luma, 255));
