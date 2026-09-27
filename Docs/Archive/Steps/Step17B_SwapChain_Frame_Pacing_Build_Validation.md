# Step 17-B.3 SwapChain Frame Pacing Build Validation

## Commit

- Frame pacing implementation: `2bb794dadd37ec6e2ee3436eaa50537258c92849`
- Waitable / device fallback contract fix: `0a35d30323e8b10637227bb65f9b6b3c03da5324`
- Validation trigger and evidence document: `881c3d1d39c138611ca091c2a9cd9187f95fc362`

## GitHub Actions

Windows Build run #1266:

- [x] Engine Debug x64
- [x] Engine Release x64
- [x] Script Debug x64
- [x] Script Release x64

Other checks:

- [x] Migration Plan Validation run #455
- [x] RHI Smoke Test run #746
- [x] D3D11 RHI Real Triangle Smoke run #47

## Runtime checks remaining

- [ ] `Frame Pacing: Waitable Object` is displayed
- [ ] Timeout count remains 0 during normal operation
- [ ] `Frame Pacing Wait` contains the regular queue wait
- [ ] `Present / Residual Queue Wait` no longer produces frequent 30–70ms spikes
- [ ] Player View and Editor View render normally
- [ ] Resize remains stable
- [ ] VSync OFF comparison
- [ ] VSync ON comparison
