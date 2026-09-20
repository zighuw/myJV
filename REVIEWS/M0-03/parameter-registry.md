# myJV 参数注册表（由 myJV_tests "[.registry]" 自动生成，请勿手改）

| ID | 名称 | 类型 | 范围 / 选项数 | 默认 |
| --- | --- | --- | --- | --- |
| `patch.common.level` | patch.common.level | Float | 0.0 - 127.0 | 127.0000000 |
| `patch.common.pan` | patch.common.pan | Float | 0.0 - 127.0 | 64.0000000 |
| `patch.common.analogFeel` | patch.common.analogFeel | Float | 0.0 - 127.0 | 0.0000000 |
| `patch.common.bendUp` | patch.common.bendUp | Int | 0 - 24 | 2 |
| `patch.common.bendDown` | patch.common.bendDown | Int | 0 - 24 | 2 |
| `patch.common.octaveShift` | patch.common.octaveShift | Int | -3 - 3 | 0 |
| `patch.common.stretchTune` | patch.common.stretchTune | Choice | 4 options | OFF |
| `patch.common.keyAssign` | patch.common.keyAssign | Choice | 2 options | POLY |
| `patch.common.legato` | patch.common.legato | Bool | Off / On | Off |
| `patch.common.portamento.switch` | patch.common.portamento.switch | Bool | Off / On | Off |
| `patch.common.portamento.mode` | patch.common.portamento.mode | Choice | 2 options | NORMAL |
| `patch.common.portamento.type` | patch.common.portamento.type | Choice | 2 options | TIME |
| `patch.common.portamento.start` | patch.common.portamento.start | Choice | 2 options | PITCH |
| `patch.common.portamento.time` | patch.common.portamento.time | Float | 0.0 - 127.0 | 64.0000000 |
| `patch.common.voicePriority` | patch.common.voicePriority | Choice | 2 options | LAST |
| `patch.common.structure12` | patch.common.structure12 | Choice | 4 options | 1 |
| `patch.common.booster12` | patch.common.booster12 | Float | 0.0 - 127.0 | 64.0000000 |
| `patch.common.structure34` | patch.common.structure34 | Choice | 4 options | 1 |
| `patch.common.booster34` | patch.common.booster34 | Float | 0.0 - 127.0 | 64.0000000 |
| `patch.common.ctrlSource2` | patch.common.ctrlSource2 | Choice | 97 options | CC11 |
| `patch.common.ctrlSource3` | patch.common.ctrlSource3 | Choice | 97 options | CC13 |
| `patch.common.controlHoldPeak` | patch.common.controlHoldPeak | Choice | 2 options | HOLD |
| `patch.common.ctrl1HoldPeak` | patch.common.ctrl1HoldPeak | Choice | 2 options | HOLD |
| `patch.common.ctrl2HoldPeak` | patch.common.ctrl2HoldPeak | Choice | 2 options | HOLD |
| `patch.common.ctrl3HoldPeak` | patch.common.ctrl3HoldPeak | Choice | 2 options | HOLD |
| `patch.common.defaultTempo` | patch.common.defaultTempo | Int | 20 - 250 | 120 |
| `tone1.wg.toneSwitch` | tone1.wg.toneSwitch | Bool | Off / On | On |
| `tone1.wg.waveGain` | tone1.wg.waveGain | Choice | 4 options | 0 dB |
| `tone1.wg.fxm.switch` | tone1.wg.fxm.switch | Bool | Off / On | Off |
| `tone1.wg.fxm.color` | tone1.wg.fxm.color | Int | 1 - 7 | 1 |
| `tone1.wg.fxm.depth` | tone1.wg.fxm.depth | Float | 0.0 - 127.0 | 0.0000000 |
| `tone1.wg.toneDelay.mode` | tone1.wg.toneDelay.mode | Choice | 3 options | NORMAL |
| `tone1.wg.toneDelay.time` | tone1.wg.toneDelay.time | Float | 0.0 - 127.0 | 0.0000000 |
| `tone1.wg.velXfade` | tone1.wg.velXfade | Float | 0.0 - 127.0 | 0.0000000 |
| `tone1.wg.velLow` | tone1.wg.velLow | Int | 1 - 127 | 1 |
| `tone1.wg.velHigh` | tone1.wg.velHigh | Int | 1 - 127 | 127 |
| `tone1.wg.keyLow` | tone1.wg.keyLow | Int | 0 - 127 | 0 |
| `tone1.wg.keyHigh` | tone1.wg.keyHigh | Int | 0 - 127 | 127 |
| `tone1.wg.redamper` | tone1.wg.redamper | Bool | Off / On | Off |
| `tone1.wg.volCtrl` | tone1.wg.volCtrl | Bool | Off / On | On |
| `tone1.wg.holdCtrl` | tone1.wg.holdCtrl | Bool | Off / On | On |
| `tone1.wg.bendCtrl` | tone1.wg.bendCtrl | Bool | Off / On | On |
| `tone1.wg.panCtrl` | tone1.wg.panCtrl | Bool | Off / On | On |
| `tone1.wg.coarseTune` | tone1.wg.coarseTune | Int | -24 - 24 | 0 |
| `tone1.wg.fineTune` | tone1.wg.fineTune | Int | -50 - 50 | 0 |
| `tone1.wg.randomPitch` | tone1.wg.randomPitch | Float | 0.0 - 127.0 | 0.0000000 |
| `tone1.wg.pitchKeyfollow` | tone1.wg.pitchKeyfollow | Float | -100.0 - 100.0 | 100.0000000 |
| `tone1.tvf.type` | tone1.tvf.type | Choice | 5 options | LPF |
| `tone1.tvf.cutoff` | tone1.tvf.cutoff | Float | 0.0 - 127.0 | 127.0000000 |
| `tone1.tvf.cutoffKeyfollow` | tone1.tvf.cutoffKeyfollow | Float | -100.0 - 100.0 | 0.0000000 |
| `tone1.tvf.resonance` | tone1.tvf.resonance | Float | 0.0 - 127.0 | 0.0000000 |
| `tone1.tvf.resVelSens` | tone1.tvf.resVelSens | Int | -63 - 63 | 0 |
| `tone1.tvf.fEnv.depth` | tone1.tvf.fEnv.depth | Int | -63 - 63 | 0 |
| `tone1.tvf.fEnv.velCurve` | tone1.tvf.fEnv.velCurve | Choice | 7 options | LINEAR |
| `tone1.tvf.fEnv.velSens` | tone1.tvf.fEnv.velSens | Int | -63 - 63 | 0 |
| `tone1.tvf.fEnv.timeKeyfollow` | tone1.tvf.fEnv.timeKeyfollow | Int | -63 - 63 | 0 |
| `tone1.tvf.fEnv.time1` | tone1.tvf.fEnv.time1 | Float | 0.0 - 127.0 | 0.0000000 |
| `tone1.tvf.fEnv.time2` | tone1.tvf.fEnv.time2 | Float | 0.0 - 127.0 | 0.0000000 |
| `tone1.tvf.fEnv.time3` | tone1.tvf.fEnv.time3 | Float | 0.0 - 127.0 | 0.0000000 |
| `tone1.tvf.fEnv.time4` | tone1.tvf.fEnv.time4 | Float | 0.0 - 127.0 | 0.0000000 |
| `tone1.tvf.fEnv.level1` | tone1.tvf.fEnv.level1 | Float | 0.0 - 127.0 | 127.0000000 |
| `tone1.tvf.fEnv.level2` | tone1.tvf.fEnv.level2 | Float | 0.0 - 127.0 | 127.0000000 |
| `tone1.tvf.fEnv.level3` | tone1.tvf.fEnv.level3 | Float | 0.0 - 127.0 | 127.0000000 |
| `tone1.tvf.fEnv.level4` | tone1.tvf.fEnv.level4 | Float | 0.0 - 127.0 | 127.0000000 |
| `tone1.tvf.fEnv.velTime1Sens` | tone1.tvf.fEnv.velTime1Sens | Int | -63 - 63 | 0 |
| `tone1.tvf.fEnv.velTime4Sens` | tone1.tvf.fEnv.velTime4Sens | Int | -63 - 63 | 0 |
| `tone1.tvf.lfo1Depth` | tone1.tvf.lfo1Depth | Float | 0.0 - 127.0 | 0.0000000 |
| `tone1.tvf.lfo2Depth` | tone1.tvf.lfo2Depth | Float | 0.0 - 127.0 | 0.0000000 |
| `tone1.tva.level` | tone1.tva.level | Float | 0.0 - 127.0 | 127.0000000 |
| `tone1.tva.bias.direction` | tone1.tva.bias.direction | Choice | 2 options | LOWER |
| `tone1.tva.bias.point` | tone1.tva.bias.point | Int | 0 - 127 | 64 |
| `tone1.tva.bias.level` | tone1.tva.bias.level | Int | -63 - 63 | 0 |
| `tone1.tva.aEnv.velCurve` | tone1.tva.aEnv.velCurve | Choice | 7 options | LINEAR |
| `tone1.tva.aEnv.velSens` | tone1.tva.aEnv.velSens | Int | -63 - 63 | 0 |
| `tone1.tva.aEnv.time1` | tone1.tva.aEnv.time1 | Float | 0.0 - 127.0 | 0.0000000 |
| `tone1.tva.aEnv.time2` | tone1.tva.aEnv.time2 | Float | 0.0 - 127.0 | 0.0000000 |
| `tone1.tva.aEnv.time3` | tone1.tva.aEnv.time3 | Float | 0.0 - 127.0 | 0.0000000 |
| `tone1.tva.aEnv.time4` | tone1.tva.aEnv.time4 | Float | 0.0 - 127.0 | 0.0000000 |
| `tone1.tva.aEnv.level1` | tone1.tva.aEnv.level1 | Float | 0.0 - 127.0 | 127.0000000 |
| `tone1.tva.aEnv.level2` | tone1.tva.aEnv.level2 | Float | 0.0 - 127.0 | 127.0000000 |
| `tone1.tva.aEnv.level3` | tone1.tva.aEnv.level3 | Float | 0.0 - 127.0 | 127.0000000 |
| `tone1.tva.aEnv.timeKeyfollow` | tone1.tva.aEnv.timeKeyfollow | Int | -63 - 63 | 0 |
| `tone1.tva.aEnv.velTime1Sens` | tone1.tva.aEnv.velTime1Sens | Int | -63 - 63 | 0 |
| `tone1.tva.aEnv.velTime4Sens` | tone1.tva.aEnv.velTime4Sens | Int | -63 - 63 | 0 |
| `tone1.tva.lfo1Depth` | tone1.tva.lfo1Depth | Float | 0.0 - 127.0 | 0.0000000 |
| `tone1.tva.lfo2Depth` | tone1.tva.lfo2Depth | Float | 0.0 - 127.0 | 0.0000000 |
| `tone1.pEnv.depth` | tone1.pEnv.depth | Int | -63 - 63 | 0 |
| `tone1.pEnv.velSens` | tone1.pEnv.velSens | Int | -63 - 63 | 0 |
| `tone1.pEnv.timeKeyfollow` | tone1.pEnv.timeKeyfollow | Int | -63 - 63 | 0 |
| `tone1.pEnv.time1` | tone1.pEnv.time1 | Float | 0.0 - 127.0 | 0.0000000 |
| `tone1.pEnv.time2` | tone1.pEnv.time2 | Float | 0.0 - 127.0 | 0.0000000 |
| `tone1.pEnv.time3` | tone1.pEnv.time3 | Float | 0.0 - 127.0 | 0.0000000 |
| `tone1.pEnv.time4` | tone1.pEnv.time4 | Float | 0.0 - 127.0 | 0.0000000 |
| `tone1.pEnv.level1` | tone1.pEnv.level1 | Float | 0.0 - 127.0 | 127.0000000 |
| `tone1.pEnv.level2` | tone1.pEnv.level2 | Float | 0.0 - 127.0 | 127.0000000 |
| `tone1.pEnv.level3` | tone1.pEnv.level3 | Float | 0.0 - 127.0 | 127.0000000 |
| `tone1.pEnv.level4` | tone1.pEnv.level4 | Float | 0.0 - 127.0 | 127.0000000 |
| `tone1.pEnv.velTime1Sens` | tone1.pEnv.velTime1Sens | Int | -63 - 63 | 0 |
| `tone1.pEnv.velTime4Sens` | tone1.pEnv.velTime4Sens | Int | -63 - 63 | 0 |
| `tone1.pan.position` | tone1.pan.position | Float | 0.0 - 127.0 | 64.0000000 |
| `tone1.pan.keyfollow` | tone1.pan.keyfollow | Int | -63 - 63 | 0 |
| `tone1.pan.random` | tone1.pan.random | Float | 0.0 - 127.0 | 0.0000000 |
| `tone1.pan.alt` | tone1.pan.alt | Float | 0.0 - 127.0 | 0.0000000 |
| `tone1.pan.lfo1Depth` | tone1.pan.lfo1Depth | Float | 0.0 - 127.0 | 0.0000000 |
| `tone1.pan.lfo2Depth` | tone1.pan.lfo2Depth | Float | 0.0 - 127.0 | 0.0000000 |
| `tone1.output.assign` | tone1.output.assign | Choice | 3 options | MAIN |
| `tone1.output.level` | tone1.output.level | Float | 0.0 - 127.0 | 127.0000000 |
| `tone1.lfo1.wave` | tone1.lfo1.wave | Choice | 8 options | SINE |
| `tone1.lfo1.keyTrig` | tone1.lfo1.keyTrig | Bool | Off / On | Off |
| `tone1.lfo1.rate` | tone1.lfo1.rate | Float | 0.0 - 127.0 | 64.0000000 |
| `tone1.lfo1.levelOffset` | tone1.lfo1.levelOffset | Int | -63 - 63 | 0 |
| `tone1.lfo1.delayTime` | tone1.lfo1.delayTime | Float | 0.0 - 127.0 | 0.0000000 |
| `tone1.lfo1.fadeMode` | tone1.lfo1.fadeMode | Choice | 2 options | LINEAR |
| `tone1.lfo1.fadeTime` | tone1.lfo1.fadeTime | Float | 0.0 - 127.0 | 0.0000000 |
| `tone1.lfo1.sync` | tone1.lfo1.sync | Bool | Off / On | Off |
| `tone1.lfo2.wave` | tone1.lfo2.wave | Choice | 8 options | SINE |
| `tone1.lfo2.keyTrig` | tone1.lfo2.keyTrig | Bool | Off / On | Off |
| `tone1.lfo2.rate` | tone1.lfo2.rate | Float | 0.0 - 127.0 | 64.0000000 |
| `tone1.lfo2.levelOffset` | tone1.lfo2.levelOffset | Int | -63 - 63 | 0 |
| `tone1.lfo2.delayTime` | tone1.lfo2.delayTime | Float | 0.0 - 127.0 | 0.0000000 |
| `tone1.lfo2.fadeMode` | tone1.lfo2.fadeMode | Choice | 2 options | LINEAR |
| `tone1.lfo2.fadeTime` | tone1.lfo2.fadeTime | Float | 0.0 - 127.0 | 0.0000000 |
| `tone1.lfo2.sync` | tone1.lfo2.sync | Bool | Off / On | Off |
| `tone1.ctrl1.dest1` | tone1.ctrl1.dest1 | Choice | 23 options | OFF |
| `tone1.ctrl1.dest2` | tone1.ctrl1.dest2 | Choice | 23 options | OFF |
| `tone1.ctrl1.dest3` | tone1.ctrl1.dest3 | Choice | 23 options | OFF |
| `tone1.ctrl1.dest4` | tone1.ctrl1.dest4 | Choice | 23 options | OFF |
| `tone1.ctrl1.depth1` | tone1.ctrl1.depth1 | Int | -63 - 63 | 0 |
| `tone1.ctrl1.depth2` | tone1.ctrl1.depth2 | Int | -63 - 63 | 0 |
| `tone1.ctrl1.depth3` | tone1.ctrl1.depth3 | Int | -63 - 63 | 0 |
| `tone1.ctrl1.depth4` | tone1.ctrl1.depth4 | Int | -63 - 63 | 0 |
| `tone1.ctrl2.dest1` | tone1.ctrl2.dest1 | Choice | 23 options | OFF |
| `tone1.ctrl2.dest2` | tone1.ctrl2.dest2 | Choice | 23 options | OFF |
| `tone1.ctrl2.dest3` | tone1.ctrl2.dest3 | Choice | 23 options | OFF |
| `tone1.ctrl2.dest4` | tone1.ctrl2.dest4 | Choice | 23 options | OFF |
| `tone1.ctrl2.depth1` | tone1.ctrl2.depth1 | Int | -63 - 63 | 0 |
| `tone1.ctrl2.depth2` | tone1.ctrl2.depth2 | Int | -63 - 63 | 0 |
| `tone1.ctrl2.depth3` | tone1.ctrl2.depth3 | Int | -63 - 63 | 0 |
| `tone1.ctrl2.depth4` | tone1.ctrl2.depth4 | Int | -63 - 63 | 0 |
| `tone1.ctrl3.dest1` | tone1.ctrl3.dest1 | Choice | 23 options | OFF |
| `tone1.ctrl3.dest2` | tone1.ctrl3.dest2 | Choice | 23 options | OFF |
| `tone1.ctrl3.dest3` | tone1.ctrl3.dest3 | Choice | 23 options | OFF |
| `tone1.ctrl3.dest4` | tone1.ctrl3.dest4 | Choice | 23 options | OFF |
| `tone1.ctrl3.depth1` | tone1.ctrl3.depth1 | Int | -63 - 63 | 0 |
| `tone1.ctrl3.depth2` | tone1.ctrl3.depth2 | Int | -63 - 63 | 0 |
| `tone1.ctrl3.depth3` | tone1.ctrl3.depth3 | Int | -63 - 63 | 0 |
| `tone1.ctrl3.depth4` | tone1.ctrl3.depth4 | Int | -63 - 63 | 0 |
| `tone2.wg.toneSwitch` | tone2.wg.toneSwitch | Bool | Off / On | On |
| `tone2.wg.waveGain` | tone2.wg.waveGain | Choice | 4 options | 0 dB |
| `tone2.wg.fxm.switch` | tone2.wg.fxm.switch | Bool | Off / On | Off |
| `tone2.wg.fxm.color` | tone2.wg.fxm.color | Int | 1 - 7 | 1 |
| `tone2.wg.fxm.depth` | tone2.wg.fxm.depth | Float | 0.0 - 127.0 | 0.0000000 |
| `tone2.wg.toneDelay.mode` | tone2.wg.toneDelay.mode | Choice | 3 options | NORMAL |
| `tone2.wg.toneDelay.time` | tone2.wg.toneDelay.time | Float | 0.0 - 127.0 | 0.0000000 |
| `tone2.wg.velXfade` | tone2.wg.velXfade | Float | 0.0 - 127.0 | 0.0000000 |
| `tone2.wg.velLow` | tone2.wg.velLow | Int | 1 - 127 | 1 |
| `tone2.wg.velHigh` | tone2.wg.velHigh | Int | 1 - 127 | 127 |
| `tone2.wg.keyLow` | tone2.wg.keyLow | Int | 0 - 127 | 0 |
| `tone2.wg.keyHigh` | tone2.wg.keyHigh | Int | 0 - 127 | 127 |
| `tone2.wg.redamper` | tone2.wg.redamper | Bool | Off / On | Off |
| `tone2.wg.volCtrl` | tone2.wg.volCtrl | Bool | Off / On | On |
| `tone2.wg.holdCtrl` | tone2.wg.holdCtrl | Bool | Off / On | On |
| `tone2.wg.bendCtrl` | tone2.wg.bendCtrl | Bool | Off / On | On |
| `tone2.wg.panCtrl` | tone2.wg.panCtrl | Bool | Off / On | On |
| `tone2.wg.coarseTune` | tone2.wg.coarseTune | Int | -24 - 24 | 0 |
| `tone2.wg.fineTune` | tone2.wg.fineTune | Int | -50 - 50 | 0 |
| `tone2.wg.randomPitch` | tone2.wg.randomPitch | Float | 0.0 - 127.0 | 0.0000000 |
| `tone2.wg.pitchKeyfollow` | tone2.wg.pitchKeyfollow | Float | -100.0 - 100.0 | 100.0000000 |
| `tone2.tvf.type` | tone2.tvf.type | Choice | 5 options | LPF |
| `tone2.tvf.cutoff` | tone2.tvf.cutoff | Float | 0.0 - 127.0 | 127.0000000 |
| `tone2.tvf.cutoffKeyfollow` | tone2.tvf.cutoffKeyfollow | Float | -100.0 - 100.0 | 0.0000000 |
| `tone2.tvf.resonance` | tone2.tvf.resonance | Float | 0.0 - 127.0 | 0.0000000 |
| `tone2.tvf.resVelSens` | tone2.tvf.resVelSens | Int | -63 - 63 | 0 |
| `tone2.tvf.fEnv.depth` | tone2.tvf.fEnv.depth | Int | -63 - 63 | 0 |
| `tone2.tvf.fEnv.velCurve` | tone2.tvf.fEnv.velCurve | Choice | 7 options | LINEAR |
| `tone2.tvf.fEnv.velSens` | tone2.tvf.fEnv.velSens | Int | -63 - 63 | 0 |
| `tone2.tvf.fEnv.timeKeyfollow` | tone2.tvf.fEnv.timeKeyfollow | Int | -63 - 63 | 0 |
| `tone2.tvf.fEnv.time1` | tone2.tvf.fEnv.time1 | Float | 0.0 - 127.0 | 0.0000000 |
| `tone2.tvf.fEnv.time2` | tone2.tvf.fEnv.time2 | Float | 0.0 - 127.0 | 0.0000000 |
| `tone2.tvf.fEnv.time3` | tone2.tvf.fEnv.time3 | Float | 0.0 - 127.0 | 0.0000000 |
| `tone2.tvf.fEnv.time4` | tone2.tvf.fEnv.time4 | Float | 0.0 - 127.0 | 0.0000000 |
| `tone2.tvf.fEnv.level1` | tone2.tvf.fEnv.level1 | Float | 0.0 - 127.0 | 127.0000000 |
| `tone2.tvf.fEnv.level2` | tone2.tvf.fEnv.level2 | Float | 0.0 - 127.0 | 127.0000000 |
| `tone2.tvf.fEnv.level3` | tone2.tvf.fEnv.level3 | Float | 0.0 - 127.0 | 127.0000000 |
| `tone2.tvf.fEnv.level4` | tone2.tvf.fEnv.level4 | Float | 0.0 - 127.0 | 127.0000000 |
| `tone2.tvf.fEnv.velTime1Sens` | tone2.tvf.fEnv.velTime1Sens | Int | -63 - 63 | 0 |
| `tone2.tvf.fEnv.velTime4Sens` | tone2.tvf.fEnv.velTime4Sens | Int | -63 - 63 | 0 |
| `tone2.tvf.lfo1Depth` | tone2.tvf.lfo1Depth | Float | 0.0 - 127.0 | 0.0000000 |
| `tone2.tvf.lfo2Depth` | tone2.tvf.lfo2Depth | Float | 0.0 - 127.0 | 0.0000000 |
| `tone2.tva.level` | tone2.tva.level | Float | 0.0 - 127.0 | 127.0000000 |
| `tone2.tva.bias.direction` | tone2.tva.bias.direction | Choice | 2 options | LOWER |
| `tone2.tva.bias.point` | tone2.tva.bias.point | Int | 0 - 127 | 64 |
| `tone2.tva.bias.level` | tone2.tva.bias.level | Int | -63 - 63 | 0 |
| `tone2.tva.aEnv.velCurve` | tone2.tva.aEnv.velCurve | Choice | 7 options | LINEAR |
| `tone2.tva.aEnv.velSens` | tone2.tva.aEnv.velSens | Int | -63 - 63 | 0 |
| `tone2.tva.aEnv.time1` | tone2.tva.aEnv.time1 | Float | 0.0 - 127.0 | 0.0000000 |
| `tone2.tva.aEnv.time2` | tone2.tva.aEnv.time2 | Float | 0.0 - 127.0 | 0.0000000 |
| `tone2.tva.aEnv.time3` | tone2.tva.aEnv.time3 | Float | 0.0 - 127.0 | 0.0000000 |
| `tone2.tva.aEnv.time4` | tone2.tva.aEnv.time4 | Float | 0.0 - 127.0 | 0.0000000 |
| `tone2.tva.aEnv.level1` | tone2.tva.aEnv.level1 | Float | 0.0 - 127.0 | 127.0000000 |
| `tone2.tva.aEnv.level2` | tone2.tva.aEnv.level2 | Float | 0.0 - 127.0 | 127.0000000 |
| `tone2.tva.aEnv.level3` | tone2.tva.aEnv.level3 | Float | 0.0 - 127.0 | 127.0000000 |
| `tone2.tva.aEnv.timeKeyfollow` | tone2.tva.aEnv.timeKeyfollow | Int | -63 - 63 | 0 |
| `tone2.tva.aEnv.velTime1Sens` | tone2.tva.aEnv.velTime1Sens | Int | -63 - 63 | 0 |
| `tone2.tva.aEnv.velTime4Sens` | tone2.tva.aEnv.velTime4Sens | Int | -63 - 63 | 0 |
| `tone2.tva.lfo1Depth` | tone2.tva.lfo1Depth | Float | 0.0 - 127.0 | 0.0000000 |
| `tone2.tva.lfo2Depth` | tone2.tva.lfo2Depth | Float | 0.0 - 127.0 | 0.0000000 |
| `tone2.pEnv.depth` | tone2.pEnv.depth | Int | -63 - 63 | 0 |
| `tone2.pEnv.velSens` | tone2.pEnv.velSens | Int | -63 - 63 | 0 |
| `tone2.pEnv.timeKeyfollow` | tone2.pEnv.timeKeyfollow | Int | -63 - 63 | 0 |
| `tone2.pEnv.time1` | tone2.pEnv.time1 | Float | 0.0 - 127.0 | 0.0000000 |
| `tone2.pEnv.time2` | tone2.pEnv.time2 | Float | 0.0 - 127.0 | 0.0000000 |
| `tone2.pEnv.time3` | tone2.pEnv.time3 | Float | 0.0 - 127.0 | 0.0000000 |
| `tone2.pEnv.time4` | tone2.pEnv.time4 | Float | 0.0 - 127.0 | 0.0000000 |
| `tone2.pEnv.level1` | tone2.pEnv.level1 | Float | 0.0 - 127.0 | 127.0000000 |
| `tone2.pEnv.level2` | tone2.pEnv.level2 | Float | 0.0 - 127.0 | 127.0000000 |
| `tone2.pEnv.level3` | tone2.pEnv.level3 | Float | 0.0 - 127.0 | 127.0000000 |
| `tone2.pEnv.level4` | tone2.pEnv.level4 | Float | 0.0 - 127.0 | 127.0000000 |
| `tone2.pEnv.velTime1Sens` | tone2.pEnv.velTime1Sens | Int | -63 - 63 | 0 |
| `tone2.pEnv.velTime4Sens` | tone2.pEnv.velTime4Sens | Int | -63 - 63 | 0 |
| `tone2.pan.position` | tone2.pan.position | Float | 0.0 - 127.0 | 64.0000000 |
| `tone2.pan.keyfollow` | tone2.pan.keyfollow | Int | -63 - 63 | 0 |
| `tone2.pan.random` | tone2.pan.random | Float | 0.0 - 127.0 | 0.0000000 |
| `tone2.pan.alt` | tone2.pan.alt | Float | 0.0 - 127.0 | 0.0000000 |
| `tone2.pan.lfo1Depth` | tone2.pan.lfo1Depth | Float | 0.0 - 127.0 | 0.0000000 |
| `tone2.pan.lfo2Depth` | tone2.pan.lfo2Depth | Float | 0.0 - 127.0 | 0.0000000 |
| `tone2.output.assign` | tone2.output.assign | Choice | 3 options | MAIN |
| `tone2.output.level` | tone2.output.level | Float | 0.0 - 127.0 | 127.0000000 |
| `tone2.lfo1.wave` | tone2.lfo1.wave | Choice | 8 options | SINE |
| `tone2.lfo1.keyTrig` | tone2.lfo1.keyTrig | Bool | Off / On | Off |
| `tone2.lfo1.rate` | tone2.lfo1.rate | Float | 0.0 - 127.0 | 64.0000000 |
| `tone2.lfo1.levelOffset` | tone2.lfo1.levelOffset | Int | -63 - 63 | 0 |
| `tone2.lfo1.delayTime` | tone2.lfo1.delayTime | Float | 0.0 - 127.0 | 0.0000000 |
| `tone2.lfo1.fadeMode` | tone2.lfo1.fadeMode | Choice | 2 options | LINEAR |
| `tone2.lfo1.fadeTime` | tone2.lfo1.fadeTime | Float | 0.0 - 127.0 | 0.0000000 |
| `tone2.lfo1.sync` | tone2.lfo1.sync | Bool | Off / On | Off |
| `tone2.lfo2.wave` | tone2.lfo2.wave | Choice | 8 options | SINE |
| `tone2.lfo2.keyTrig` | tone2.lfo2.keyTrig | Bool | Off / On | Off |
| `tone2.lfo2.rate` | tone2.lfo2.rate | Float | 0.0 - 127.0 | 64.0000000 |
| `tone2.lfo2.levelOffset` | tone2.lfo2.levelOffset | Int | -63 - 63 | 0 |
| `tone2.lfo2.delayTime` | tone2.lfo2.delayTime | Float | 0.0 - 127.0 | 0.0000000 |
| `tone2.lfo2.fadeMode` | tone2.lfo2.fadeMode | Choice | 2 options | LINEAR |
| `tone2.lfo2.fadeTime` | tone2.lfo2.fadeTime | Float | 0.0 - 127.0 | 0.0000000 |
| `tone2.lfo2.sync` | tone2.lfo2.sync | Bool | Off / On | Off |
| `tone2.ctrl1.dest1` | tone2.ctrl1.dest1 | Choice | 23 options | OFF |
| `tone2.ctrl1.dest2` | tone2.ctrl1.dest2 | Choice | 23 options | OFF |
| `tone2.ctrl1.dest3` | tone2.ctrl1.dest3 | Choice | 23 options | OFF |
| `tone2.ctrl1.dest4` | tone2.ctrl1.dest4 | Choice | 23 options | OFF |
| `tone2.ctrl1.depth1` | tone2.ctrl1.depth1 | Int | -63 - 63 | 0 |
| `tone2.ctrl1.depth2` | tone2.ctrl1.depth2 | Int | -63 - 63 | 0 |
| `tone2.ctrl1.depth3` | tone2.ctrl1.depth3 | Int | -63 - 63 | 0 |
| `tone2.ctrl1.depth4` | tone2.ctrl1.depth4 | Int | -63 - 63 | 0 |
| `tone2.ctrl2.dest1` | tone2.ctrl2.dest1 | Choice | 23 options | OFF |
| `tone2.ctrl2.dest2` | tone2.ctrl2.dest2 | Choice | 23 options | OFF |
| `tone2.ctrl2.dest3` | tone2.ctrl2.dest3 | Choice | 23 options | OFF |
| `tone2.ctrl2.dest4` | tone2.ctrl2.dest4 | Choice | 23 options | OFF |
| `tone2.ctrl2.depth1` | tone2.ctrl2.depth1 | Int | -63 - 63 | 0 |
| `tone2.ctrl2.depth2` | tone2.ctrl2.depth2 | Int | -63 - 63 | 0 |
| `tone2.ctrl2.depth3` | tone2.ctrl2.depth3 | Int | -63 - 63 | 0 |
| `tone2.ctrl2.depth4` | tone2.ctrl2.depth4 | Int | -63 - 63 | 0 |
| `tone2.ctrl3.dest1` | tone2.ctrl3.dest1 | Choice | 23 options | OFF |
| `tone2.ctrl3.dest2` | tone2.ctrl3.dest2 | Choice | 23 options | OFF |
| `tone2.ctrl3.dest3` | tone2.ctrl3.dest3 | Choice | 23 options | OFF |
| `tone2.ctrl3.dest4` | tone2.ctrl3.dest4 | Choice | 23 options | OFF |
| `tone2.ctrl3.depth1` | tone2.ctrl3.depth1 | Int | -63 - 63 | 0 |
| `tone2.ctrl3.depth2` | tone2.ctrl3.depth2 | Int | -63 - 63 | 0 |
| `tone2.ctrl3.depth3` | tone2.ctrl3.depth3 | Int | -63 - 63 | 0 |
| `tone2.ctrl3.depth4` | tone2.ctrl3.depth4 | Int | -63 - 63 | 0 |
| `tone3.wg.toneSwitch` | tone3.wg.toneSwitch | Bool | Off / On | On |
| `tone3.wg.waveGain` | tone3.wg.waveGain | Choice | 4 options | 0 dB |
| `tone3.wg.fxm.switch` | tone3.wg.fxm.switch | Bool | Off / On | Off |
| `tone3.wg.fxm.color` | tone3.wg.fxm.color | Int | 1 - 7 | 1 |
| `tone3.wg.fxm.depth` | tone3.wg.fxm.depth | Float | 0.0 - 127.0 | 0.0000000 |
| `tone3.wg.toneDelay.mode` | tone3.wg.toneDelay.mode | Choice | 3 options | NORMAL |
| `tone3.wg.toneDelay.time` | tone3.wg.toneDelay.time | Float | 0.0 - 127.0 | 0.0000000 |
| `tone3.wg.velXfade` | tone3.wg.velXfade | Float | 0.0 - 127.0 | 0.0000000 |
| `tone3.wg.velLow` | tone3.wg.velLow | Int | 1 - 127 | 1 |
| `tone3.wg.velHigh` | tone3.wg.velHigh | Int | 1 - 127 | 127 |
| `tone3.wg.keyLow` | tone3.wg.keyLow | Int | 0 - 127 | 0 |
| `tone3.wg.keyHigh` | tone3.wg.keyHigh | Int | 0 - 127 | 127 |
| `tone3.wg.redamper` | tone3.wg.redamper | Bool | Off / On | Off |
| `tone3.wg.volCtrl` | tone3.wg.volCtrl | Bool | Off / On | On |
| `tone3.wg.holdCtrl` | tone3.wg.holdCtrl | Bool | Off / On | On |
| `tone3.wg.bendCtrl` | tone3.wg.bendCtrl | Bool | Off / On | On |
| `tone3.wg.panCtrl` | tone3.wg.panCtrl | Bool | Off / On | On |
| `tone3.wg.coarseTune` | tone3.wg.coarseTune | Int | -24 - 24 | 0 |
| `tone3.wg.fineTune` | tone3.wg.fineTune | Int | -50 - 50 | 0 |
| `tone3.wg.randomPitch` | tone3.wg.randomPitch | Float | 0.0 - 127.0 | 0.0000000 |
| `tone3.wg.pitchKeyfollow` | tone3.wg.pitchKeyfollow | Float | -100.0 - 100.0 | 100.0000000 |
| `tone3.tvf.type` | tone3.tvf.type | Choice | 5 options | LPF |
| `tone3.tvf.cutoff` | tone3.tvf.cutoff | Float | 0.0 - 127.0 | 127.0000000 |
| `tone3.tvf.cutoffKeyfollow` | tone3.tvf.cutoffKeyfollow | Float | -100.0 - 100.0 | 0.0000000 |
| `tone3.tvf.resonance` | tone3.tvf.resonance | Float | 0.0 - 127.0 | 0.0000000 |
| `tone3.tvf.resVelSens` | tone3.tvf.resVelSens | Int | -63 - 63 | 0 |
| `tone3.tvf.fEnv.depth` | tone3.tvf.fEnv.depth | Int | -63 - 63 | 0 |
| `tone3.tvf.fEnv.velCurve` | tone3.tvf.fEnv.velCurve | Choice | 7 options | LINEAR |
| `tone3.tvf.fEnv.velSens` | tone3.tvf.fEnv.velSens | Int | -63 - 63 | 0 |
| `tone3.tvf.fEnv.timeKeyfollow` | tone3.tvf.fEnv.timeKeyfollow | Int | -63 - 63 | 0 |
| `tone3.tvf.fEnv.time1` | tone3.tvf.fEnv.time1 | Float | 0.0 - 127.0 | 0.0000000 |
| `tone3.tvf.fEnv.time2` | tone3.tvf.fEnv.time2 | Float | 0.0 - 127.0 | 0.0000000 |
| `tone3.tvf.fEnv.time3` | tone3.tvf.fEnv.time3 | Float | 0.0 - 127.0 | 0.0000000 |
| `tone3.tvf.fEnv.time4` | tone3.tvf.fEnv.time4 | Float | 0.0 - 127.0 | 0.0000000 |
| `tone3.tvf.fEnv.level1` | tone3.tvf.fEnv.level1 | Float | 0.0 - 127.0 | 127.0000000 |
| `tone3.tvf.fEnv.level2` | tone3.tvf.fEnv.level2 | Float | 0.0 - 127.0 | 127.0000000 |
| `tone3.tvf.fEnv.level3` | tone3.tvf.fEnv.level3 | Float | 0.0 - 127.0 | 127.0000000 |
| `tone3.tvf.fEnv.level4` | tone3.tvf.fEnv.level4 | Float | 0.0 - 127.0 | 127.0000000 |
| `tone3.tvf.fEnv.velTime1Sens` | tone3.tvf.fEnv.velTime1Sens | Int | -63 - 63 | 0 |
| `tone3.tvf.fEnv.velTime4Sens` | tone3.tvf.fEnv.velTime4Sens | Int | -63 - 63 | 0 |
| `tone3.tvf.lfo1Depth` | tone3.tvf.lfo1Depth | Float | 0.0 - 127.0 | 0.0000000 |
| `tone3.tvf.lfo2Depth` | tone3.tvf.lfo2Depth | Float | 0.0 - 127.0 | 0.0000000 |
| `tone3.tva.level` | tone3.tva.level | Float | 0.0 - 127.0 | 127.0000000 |
| `tone3.tva.bias.direction` | tone3.tva.bias.direction | Choice | 2 options | LOWER |
| `tone3.tva.bias.point` | tone3.tva.bias.point | Int | 0 - 127 | 64 |
| `tone3.tva.bias.level` | tone3.tva.bias.level | Int | -63 - 63 | 0 |
| `tone3.tva.aEnv.velCurve` | tone3.tva.aEnv.velCurve | Choice | 7 options | LINEAR |
| `tone3.tva.aEnv.velSens` | tone3.tva.aEnv.velSens | Int | -63 - 63 | 0 |
| `tone3.tva.aEnv.time1` | tone3.tva.aEnv.time1 | Float | 0.0 - 127.0 | 0.0000000 |
| `tone3.tva.aEnv.time2` | tone3.tva.aEnv.time2 | Float | 0.0 - 127.0 | 0.0000000 |
| `tone3.tva.aEnv.time3` | tone3.tva.aEnv.time3 | Float | 0.0 - 127.0 | 0.0000000 |
| `tone3.tva.aEnv.time4` | tone3.tva.aEnv.time4 | Float | 0.0 - 127.0 | 0.0000000 |
| `tone3.tva.aEnv.level1` | tone3.tva.aEnv.level1 | Float | 0.0 - 127.0 | 127.0000000 |
| `tone3.tva.aEnv.level2` | tone3.tva.aEnv.level2 | Float | 0.0 - 127.0 | 127.0000000 |
| `tone3.tva.aEnv.level3` | tone3.tva.aEnv.level3 | Float | 0.0 - 127.0 | 127.0000000 |
| `tone3.tva.aEnv.timeKeyfollow` | tone3.tva.aEnv.timeKeyfollow | Int | -63 - 63 | 0 |
| `tone3.tva.aEnv.velTime1Sens` | tone3.tva.aEnv.velTime1Sens | Int | -63 - 63 | 0 |
| `tone3.tva.aEnv.velTime4Sens` | tone3.tva.aEnv.velTime4Sens | Int | -63 - 63 | 0 |
| `tone3.tva.lfo1Depth` | tone3.tva.lfo1Depth | Float | 0.0 - 127.0 | 0.0000000 |
| `tone3.tva.lfo2Depth` | tone3.tva.lfo2Depth | Float | 0.0 - 127.0 | 0.0000000 |
| `tone3.pEnv.depth` | tone3.pEnv.depth | Int | -63 - 63 | 0 |
| `tone3.pEnv.velSens` | tone3.pEnv.velSens | Int | -63 - 63 | 0 |
| `tone3.pEnv.timeKeyfollow` | tone3.pEnv.timeKeyfollow | Int | -63 - 63 | 0 |
| `tone3.pEnv.time1` | tone3.pEnv.time1 | Float | 0.0 - 127.0 | 0.0000000 |
| `tone3.pEnv.time2` | tone3.pEnv.time2 | Float | 0.0 - 127.0 | 0.0000000 |
| `tone3.pEnv.time3` | tone3.pEnv.time3 | Float | 0.0 - 127.0 | 0.0000000 |
| `tone3.pEnv.time4` | tone3.pEnv.time4 | Float | 0.0 - 127.0 | 0.0000000 |
| `tone3.pEnv.level1` | tone3.pEnv.level1 | Float | 0.0 - 127.0 | 127.0000000 |
| `tone3.pEnv.level2` | tone3.pEnv.level2 | Float | 0.0 - 127.0 | 127.0000000 |
| `tone3.pEnv.level3` | tone3.pEnv.level3 | Float | 0.0 - 127.0 | 127.0000000 |
| `tone3.pEnv.level4` | tone3.pEnv.level4 | Float | 0.0 - 127.0 | 127.0000000 |
| `tone3.pEnv.velTime1Sens` | tone3.pEnv.velTime1Sens | Int | -63 - 63 | 0 |
| `tone3.pEnv.velTime4Sens` | tone3.pEnv.velTime4Sens | Int | -63 - 63 | 0 |
| `tone3.pan.position` | tone3.pan.position | Float | 0.0 - 127.0 | 64.0000000 |
| `tone3.pan.keyfollow` | tone3.pan.keyfollow | Int | -63 - 63 | 0 |
| `tone3.pan.random` | tone3.pan.random | Float | 0.0 - 127.0 | 0.0000000 |
| `tone3.pan.alt` | tone3.pan.alt | Float | 0.0 - 127.0 | 0.0000000 |
| `tone3.pan.lfo1Depth` | tone3.pan.lfo1Depth | Float | 0.0 - 127.0 | 0.0000000 |
| `tone3.pan.lfo2Depth` | tone3.pan.lfo2Depth | Float | 0.0 - 127.0 | 0.0000000 |
| `tone3.output.assign` | tone3.output.assign | Choice | 3 options | MAIN |
| `tone3.output.level` | tone3.output.level | Float | 0.0 - 127.0 | 127.0000000 |
| `tone3.lfo1.wave` | tone3.lfo1.wave | Choice | 8 options | SINE |
| `tone3.lfo1.keyTrig` | tone3.lfo1.keyTrig | Bool | Off / On | Off |
| `tone3.lfo1.rate` | tone3.lfo1.rate | Float | 0.0 - 127.0 | 64.0000000 |
| `tone3.lfo1.levelOffset` | tone3.lfo1.levelOffset | Int | -63 - 63 | 0 |
| `tone3.lfo1.delayTime` | tone3.lfo1.delayTime | Float | 0.0 - 127.0 | 0.0000000 |
| `tone3.lfo1.fadeMode` | tone3.lfo1.fadeMode | Choice | 2 options | LINEAR |
| `tone3.lfo1.fadeTime` | tone3.lfo1.fadeTime | Float | 0.0 - 127.0 | 0.0000000 |
| `tone3.lfo1.sync` | tone3.lfo1.sync | Bool | Off / On | Off |
| `tone3.lfo2.wave` | tone3.lfo2.wave | Choice | 8 options | SINE |
| `tone3.lfo2.keyTrig` | tone3.lfo2.keyTrig | Bool | Off / On | Off |
| `tone3.lfo2.rate` | tone3.lfo2.rate | Float | 0.0 - 127.0 | 64.0000000 |
| `tone3.lfo2.levelOffset` | tone3.lfo2.levelOffset | Int | -63 - 63 | 0 |
| `tone3.lfo2.delayTime` | tone3.lfo2.delayTime | Float | 0.0 - 127.0 | 0.0000000 |
| `tone3.lfo2.fadeMode` | tone3.lfo2.fadeMode | Choice | 2 options | LINEAR |
| `tone3.lfo2.fadeTime` | tone3.lfo2.fadeTime | Float | 0.0 - 127.0 | 0.0000000 |
| `tone3.lfo2.sync` | tone3.lfo2.sync | Bool | Off / On | Off |
| `tone3.ctrl1.dest1` | tone3.ctrl1.dest1 | Choice | 23 options | OFF |
| `tone3.ctrl1.dest2` | tone3.ctrl1.dest2 | Choice | 23 options | OFF |
| `tone3.ctrl1.dest3` | tone3.ctrl1.dest3 | Choice | 23 options | OFF |
| `tone3.ctrl1.dest4` | tone3.ctrl1.dest4 | Choice | 23 options | OFF |
| `tone3.ctrl1.depth1` | tone3.ctrl1.depth1 | Int | -63 - 63 | 0 |
| `tone3.ctrl1.depth2` | tone3.ctrl1.depth2 | Int | -63 - 63 | 0 |
| `tone3.ctrl1.depth3` | tone3.ctrl1.depth3 | Int | -63 - 63 | 0 |
| `tone3.ctrl1.depth4` | tone3.ctrl1.depth4 | Int | -63 - 63 | 0 |
| `tone3.ctrl2.dest1` | tone3.ctrl2.dest1 | Choice | 23 options | OFF |
| `tone3.ctrl2.dest2` | tone3.ctrl2.dest2 | Choice | 23 options | OFF |
| `tone3.ctrl2.dest3` | tone3.ctrl2.dest3 | Choice | 23 options | OFF |
| `tone3.ctrl2.dest4` | tone3.ctrl2.dest4 | Choice | 23 options | OFF |
| `tone3.ctrl2.depth1` | tone3.ctrl2.depth1 | Int | -63 - 63 | 0 |
| `tone3.ctrl2.depth2` | tone3.ctrl2.depth2 | Int | -63 - 63 | 0 |
| `tone3.ctrl2.depth3` | tone3.ctrl2.depth3 | Int | -63 - 63 | 0 |
| `tone3.ctrl2.depth4` | tone3.ctrl2.depth4 | Int | -63 - 63 | 0 |
| `tone3.ctrl3.dest1` | tone3.ctrl3.dest1 | Choice | 23 options | OFF |
| `tone3.ctrl3.dest2` | tone3.ctrl3.dest2 | Choice | 23 options | OFF |
| `tone3.ctrl3.dest3` | tone3.ctrl3.dest3 | Choice | 23 options | OFF |
| `tone3.ctrl3.dest4` | tone3.ctrl3.dest4 | Choice | 23 options | OFF |
| `tone3.ctrl3.depth1` | tone3.ctrl3.depth1 | Int | -63 - 63 | 0 |
| `tone3.ctrl3.depth2` | tone3.ctrl3.depth2 | Int | -63 - 63 | 0 |
| `tone3.ctrl3.depth3` | tone3.ctrl3.depth3 | Int | -63 - 63 | 0 |
| `tone3.ctrl3.depth4` | tone3.ctrl3.depth4 | Int | -63 - 63 | 0 |
| `tone4.wg.toneSwitch` | tone4.wg.toneSwitch | Bool | Off / On | On |
| `tone4.wg.waveGain` | tone4.wg.waveGain | Choice | 4 options | 0 dB |
| `tone4.wg.fxm.switch` | tone4.wg.fxm.switch | Bool | Off / On | Off |
| `tone4.wg.fxm.color` | tone4.wg.fxm.color | Int | 1 - 7 | 1 |
| `tone4.wg.fxm.depth` | tone4.wg.fxm.depth | Float | 0.0 - 127.0 | 0.0000000 |
| `tone4.wg.toneDelay.mode` | tone4.wg.toneDelay.mode | Choice | 3 options | NORMAL |
| `tone4.wg.toneDelay.time` | tone4.wg.toneDelay.time | Float | 0.0 - 127.0 | 0.0000000 |
| `tone4.wg.velXfade` | tone4.wg.velXfade | Float | 0.0 - 127.0 | 0.0000000 |
| `tone4.wg.velLow` | tone4.wg.velLow | Int | 1 - 127 | 1 |
| `tone4.wg.velHigh` | tone4.wg.velHigh | Int | 1 - 127 | 127 |
| `tone4.wg.keyLow` | tone4.wg.keyLow | Int | 0 - 127 | 0 |
| `tone4.wg.keyHigh` | tone4.wg.keyHigh | Int | 0 - 127 | 127 |
| `tone4.wg.redamper` | tone4.wg.redamper | Bool | Off / On | Off |
| `tone4.wg.volCtrl` | tone4.wg.volCtrl | Bool | Off / On | On |
| `tone4.wg.holdCtrl` | tone4.wg.holdCtrl | Bool | Off / On | On |
| `tone4.wg.bendCtrl` | tone4.wg.bendCtrl | Bool | Off / On | On |
| `tone4.wg.panCtrl` | tone4.wg.panCtrl | Bool | Off / On | On |
| `tone4.wg.coarseTune` | tone4.wg.coarseTune | Int | -24 - 24 | 0 |
| `tone4.wg.fineTune` | tone4.wg.fineTune | Int | -50 - 50 | 0 |
| `tone4.wg.randomPitch` | tone4.wg.randomPitch | Float | 0.0 - 127.0 | 0.0000000 |
| `tone4.wg.pitchKeyfollow` | tone4.wg.pitchKeyfollow | Float | -100.0 - 100.0 | 100.0000000 |
| `tone4.tvf.type` | tone4.tvf.type | Choice | 5 options | LPF |
| `tone4.tvf.cutoff` | tone4.tvf.cutoff | Float | 0.0 - 127.0 | 127.0000000 |
| `tone4.tvf.cutoffKeyfollow` | tone4.tvf.cutoffKeyfollow | Float | -100.0 - 100.0 | 0.0000000 |
| `tone4.tvf.resonance` | tone4.tvf.resonance | Float | 0.0 - 127.0 | 0.0000000 |
| `tone4.tvf.resVelSens` | tone4.tvf.resVelSens | Int | -63 - 63 | 0 |
| `tone4.tvf.fEnv.depth` | tone4.tvf.fEnv.depth | Int | -63 - 63 | 0 |
| `tone4.tvf.fEnv.velCurve` | tone4.tvf.fEnv.velCurve | Choice | 7 options | LINEAR |
| `tone4.tvf.fEnv.velSens` | tone4.tvf.fEnv.velSens | Int | -63 - 63 | 0 |
| `tone4.tvf.fEnv.timeKeyfollow` | tone4.tvf.fEnv.timeKeyfollow | Int | -63 - 63 | 0 |
| `tone4.tvf.fEnv.time1` | tone4.tvf.fEnv.time1 | Float | 0.0 - 127.0 | 0.0000000 |
| `tone4.tvf.fEnv.time2` | tone4.tvf.fEnv.time2 | Float | 0.0 - 127.0 | 0.0000000 |
| `tone4.tvf.fEnv.time3` | tone4.tvf.fEnv.time3 | Float | 0.0 - 127.0 | 0.0000000 |
| `tone4.tvf.fEnv.time4` | tone4.tvf.fEnv.time4 | Float | 0.0 - 127.0 | 0.0000000 |
| `tone4.tvf.fEnv.level1` | tone4.tvf.fEnv.level1 | Float | 0.0 - 127.0 | 127.0000000 |
| `tone4.tvf.fEnv.level2` | tone4.tvf.fEnv.level2 | Float | 0.0 - 127.0 | 127.0000000 |
| `tone4.tvf.fEnv.level3` | tone4.tvf.fEnv.level3 | Float | 0.0 - 127.0 | 127.0000000 |
| `tone4.tvf.fEnv.level4` | tone4.tvf.fEnv.level4 | Float | 0.0 - 127.0 | 127.0000000 |
| `tone4.tvf.fEnv.velTime1Sens` | tone4.tvf.fEnv.velTime1Sens | Int | -63 - 63 | 0 |
| `tone4.tvf.fEnv.velTime4Sens` | tone4.tvf.fEnv.velTime4Sens | Int | -63 - 63 | 0 |
| `tone4.tvf.lfo1Depth` | tone4.tvf.lfo1Depth | Float | 0.0 - 127.0 | 0.0000000 |
| `tone4.tvf.lfo2Depth` | tone4.tvf.lfo2Depth | Float | 0.0 - 127.0 | 0.0000000 |
| `tone4.tva.level` | tone4.tva.level | Float | 0.0 - 127.0 | 127.0000000 |
| `tone4.tva.bias.direction` | tone4.tva.bias.direction | Choice | 2 options | LOWER |
| `tone4.tva.bias.point` | tone4.tva.bias.point | Int | 0 - 127 | 64 |
| `tone4.tva.bias.level` | tone4.tva.bias.level | Int | -63 - 63 | 0 |
| `tone4.tva.aEnv.velCurve` | tone4.tva.aEnv.velCurve | Choice | 7 options | LINEAR |
| `tone4.tva.aEnv.velSens` | tone4.tva.aEnv.velSens | Int | -63 - 63 | 0 |
| `tone4.tva.aEnv.time1` | tone4.tva.aEnv.time1 | Float | 0.0 - 127.0 | 0.0000000 |
| `tone4.tva.aEnv.time2` | tone4.tva.aEnv.time2 | Float | 0.0 - 127.0 | 0.0000000 |
| `tone4.tva.aEnv.time3` | tone4.tva.aEnv.time3 | Float | 0.0 - 127.0 | 0.0000000 |
| `tone4.tva.aEnv.time4` | tone4.tva.aEnv.time4 | Float | 0.0 - 127.0 | 0.0000000 |
| `tone4.tva.aEnv.level1` | tone4.tva.aEnv.level1 | Float | 0.0 - 127.0 | 127.0000000 |
| `tone4.tva.aEnv.level2` | tone4.tva.aEnv.level2 | Float | 0.0 - 127.0 | 127.0000000 |
| `tone4.tva.aEnv.level3` | tone4.tva.aEnv.level3 | Float | 0.0 - 127.0 | 127.0000000 |
| `tone4.tva.aEnv.timeKeyfollow` | tone4.tva.aEnv.timeKeyfollow | Int | -63 - 63 | 0 |
| `tone4.tva.aEnv.velTime1Sens` | tone4.tva.aEnv.velTime1Sens | Int | -63 - 63 | 0 |
| `tone4.tva.aEnv.velTime4Sens` | tone4.tva.aEnv.velTime4Sens | Int | -63 - 63 | 0 |
| `tone4.tva.lfo1Depth` | tone4.tva.lfo1Depth | Float | 0.0 - 127.0 | 0.0000000 |
| `tone4.tva.lfo2Depth` | tone4.tva.lfo2Depth | Float | 0.0 - 127.0 | 0.0000000 |
| `tone4.pEnv.depth` | tone4.pEnv.depth | Int | -63 - 63 | 0 |
| `tone4.pEnv.velSens` | tone4.pEnv.velSens | Int | -63 - 63 | 0 |
| `tone4.pEnv.timeKeyfollow` | tone4.pEnv.timeKeyfollow | Int | -63 - 63 | 0 |
| `tone4.pEnv.time1` | tone4.pEnv.time1 | Float | 0.0 - 127.0 | 0.0000000 |
| `tone4.pEnv.time2` | tone4.pEnv.time2 | Float | 0.0 - 127.0 | 0.0000000 |
| `tone4.pEnv.time3` | tone4.pEnv.time3 | Float | 0.0 - 127.0 | 0.0000000 |
| `tone4.pEnv.time4` | tone4.pEnv.time4 | Float | 0.0 - 127.0 | 0.0000000 |
| `tone4.pEnv.level1` | tone4.pEnv.level1 | Float | 0.0 - 127.0 | 127.0000000 |
| `tone4.pEnv.level2` | tone4.pEnv.level2 | Float | 0.0 - 127.0 | 127.0000000 |
| `tone4.pEnv.level3` | tone4.pEnv.level3 | Float | 0.0 - 127.0 | 127.0000000 |
| `tone4.pEnv.level4` | tone4.pEnv.level4 | Float | 0.0 - 127.0 | 127.0000000 |
| `tone4.pEnv.velTime1Sens` | tone4.pEnv.velTime1Sens | Int | -63 - 63 | 0 |
| `tone4.pEnv.velTime4Sens` | tone4.pEnv.velTime4Sens | Int | -63 - 63 | 0 |
| `tone4.pan.position` | tone4.pan.position | Float | 0.0 - 127.0 | 64.0000000 |
| `tone4.pan.keyfollow` | tone4.pan.keyfollow | Int | -63 - 63 | 0 |
| `tone4.pan.random` | tone4.pan.random | Float | 0.0 - 127.0 | 0.0000000 |
| `tone4.pan.alt` | tone4.pan.alt | Float | 0.0 - 127.0 | 0.0000000 |
| `tone4.pan.lfo1Depth` | tone4.pan.lfo1Depth | Float | 0.0 - 127.0 | 0.0000000 |
| `tone4.pan.lfo2Depth` | tone4.pan.lfo2Depth | Float | 0.0 - 127.0 | 0.0000000 |
| `tone4.output.assign` | tone4.output.assign | Choice | 3 options | MAIN |
| `tone4.output.level` | tone4.output.level | Float | 0.0 - 127.0 | 127.0000000 |
| `tone4.lfo1.wave` | tone4.lfo1.wave | Choice | 8 options | SINE |
| `tone4.lfo1.keyTrig` | tone4.lfo1.keyTrig | Bool | Off / On | Off |
| `tone4.lfo1.rate` | tone4.lfo1.rate | Float | 0.0 - 127.0 | 64.0000000 |
| `tone4.lfo1.levelOffset` | tone4.lfo1.levelOffset | Int | -63 - 63 | 0 |
| `tone4.lfo1.delayTime` | tone4.lfo1.delayTime | Float | 0.0 - 127.0 | 0.0000000 |
| `tone4.lfo1.fadeMode` | tone4.lfo1.fadeMode | Choice | 2 options | LINEAR |
| `tone4.lfo1.fadeTime` | tone4.lfo1.fadeTime | Float | 0.0 - 127.0 | 0.0000000 |
| `tone4.lfo1.sync` | tone4.lfo1.sync | Bool | Off / On | Off |
| `tone4.lfo2.wave` | tone4.lfo2.wave | Choice | 8 options | SINE |
| `tone4.lfo2.keyTrig` | tone4.lfo2.keyTrig | Bool | Off / On | Off |
| `tone4.lfo2.rate` | tone4.lfo2.rate | Float | 0.0 - 127.0 | 64.0000000 |
| `tone4.lfo2.levelOffset` | tone4.lfo2.levelOffset | Int | -63 - 63 | 0 |
| `tone4.lfo2.delayTime` | tone4.lfo2.delayTime | Float | 0.0 - 127.0 | 0.0000000 |
| `tone4.lfo2.fadeMode` | tone4.lfo2.fadeMode | Choice | 2 options | LINEAR |
| `tone4.lfo2.fadeTime` | tone4.lfo2.fadeTime | Float | 0.0 - 127.0 | 0.0000000 |
| `tone4.lfo2.sync` | tone4.lfo2.sync | Bool | Off / On | Off |
| `tone4.ctrl1.dest1` | tone4.ctrl1.dest1 | Choice | 23 options | OFF |
| `tone4.ctrl1.dest2` | tone4.ctrl1.dest2 | Choice | 23 options | OFF |
| `tone4.ctrl1.dest3` | tone4.ctrl1.dest3 | Choice | 23 options | OFF |
| `tone4.ctrl1.dest4` | tone4.ctrl1.dest4 | Choice | 23 options | OFF |
| `tone4.ctrl1.depth1` | tone4.ctrl1.depth1 | Int | -63 - 63 | 0 |
| `tone4.ctrl1.depth2` | tone4.ctrl1.depth2 | Int | -63 - 63 | 0 |
| `tone4.ctrl1.depth3` | tone4.ctrl1.depth3 | Int | -63 - 63 | 0 |
| `tone4.ctrl1.depth4` | tone4.ctrl1.depth4 | Int | -63 - 63 | 0 |
| `tone4.ctrl2.dest1` | tone4.ctrl2.dest1 | Choice | 23 options | OFF |
| `tone4.ctrl2.dest2` | tone4.ctrl2.dest2 | Choice | 23 options | OFF |
| `tone4.ctrl2.dest3` | tone4.ctrl2.dest3 | Choice | 23 options | OFF |
| `tone4.ctrl2.dest4` | tone4.ctrl2.dest4 | Choice | 23 options | OFF |
| `tone4.ctrl2.depth1` | tone4.ctrl2.depth1 | Int | -63 - 63 | 0 |
| `tone4.ctrl2.depth2` | tone4.ctrl2.depth2 | Int | -63 - 63 | 0 |
| `tone4.ctrl2.depth3` | tone4.ctrl2.depth3 | Int | -63 - 63 | 0 |
| `tone4.ctrl2.depth4` | tone4.ctrl2.depth4 | Int | -63 - 63 | 0 |
| `tone4.ctrl3.dest1` | tone4.ctrl3.dest1 | Choice | 23 options | OFF |
| `tone4.ctrl3.dest2` | tone4.ctrl3.dest2 | Choice | 23 options | OFF |
| `tone4.ctrl3.dest3` | tone4.ctrl3.dest3 | Choice | 23 options | OFF |
| `tone4.ctrl3.dest4` | tone4.ctrl3.dest4 | Choice | 23 options | OFF |
| `tone4.ctrl3.depth1` | tone4.ctrl3.depth1 | Int | -63 - 63 | 0 |
| `tone4.ctrl3.depth2` | tone4.ctrl3.depth2 | Int | -63 - 63 | 0 |
| `tone4.ctrl3.depth3` | tone4.ctrl3.depth3 | Int | -63 - 63 | 0 |
| `tone4.ctrl3.depth4` | tone4.ctrl3.depth4 | Int | -63 - 63 | 0 |

