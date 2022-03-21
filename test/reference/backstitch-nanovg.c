nvgBeginPath(contexto_gráfico);
nvgRect(contexto_gráfico, 100,100, 120,30);
nvgCircle(contexto_gráfico, 120,120, 5);
nvgPathWinding(contexto_gráfico, NVG_HOLE);
nvgFillColor(contexto_gráfico, nvgRGBA(255,192,0,255));
nvgFill(contexto_gráfico);
