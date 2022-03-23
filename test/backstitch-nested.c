#pragma Cedro 1.0
    NVGpaint sky =
        vg @nvg...
        LinearGradient(0, 0, 0, horizon,
                       @nvg...
                       HSLA(0.60, 1.00, 0.20*luma, 255),
                       HSLA(0.60, 1.00, 0.90*luma, 255));
    NVGpaint ground =
        vg @nvg...
        LinearGradient(0, horizon, 0, fbHeight,
                       @nvg...
                       HSLA(0.34, 0.73, 0.20*luma, 255),
                       HSLA(0.34, 0.73, 0.67*luma, 255));
