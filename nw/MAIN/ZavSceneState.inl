CViewScene* pScene = 0;
SGRViewport** ppViewports = 0;

SGRViewport* ZAV_Viewport() {
  return ppViewports != 0 ? ppViewports[0] : 0;
}

CViewScene* ZAV_Scene() {
  return pScene;
}
