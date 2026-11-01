/**
 * ===================================================================
 * VORTEX-X TACTICAL GAMEPAD MISSION CONTROL // ENGINE JAVASCRIPT
 * Full Web Gamepad API, Dual Virtual Joysticks, Synthesized Audio,
 * 8-Servo Kinematic Animation, and PIR Surveillance Telemetry.
 * ===================================================================
 */

(function () {
  'use strict';

  // --- STATE SYSTEM ---
  const state = {
    connected: false,
    simMode: true,
    audioEnabled: true,
    espIp: '192.168.4.1',
    lastPing: 4,
    currentCommand: 'STAND',
    currentSpeed: 2, // 1: ECO, 2: NORMAL, 3: SPORT
    activeGamepads: {},
    selectedGamepadIndex: null,
    
    // Telemetry Values
    telemetry: {
      distance: 42.5,
      temperature: 28.4,
      humidity: 65.0,
      pirState: 0, // 0 = Idle, 1 = Motion Detected
      pirRaw: 0,
      aiScore: 0.94,
      aiVerdict: 'NORMAL',
      gaitState: 'STAND',
      safetyStatus: 'SAFE'
    },

    // 8 Servo Angles (Degrees: 0-180)
    servos: {
      flHip: 90, flLift: 90,
      frHip: 90, frLift: 90,
      rlHip: 90, rlLift: 90,
      rrHip: 90, rrLift: 90
    },

    // Gait animation cycle
    gaitStep: 0,
    gaitInterval: null,

    // Virtual Joysticks
    joyLeft: { x: 0, y: 0, active: false, id: null },
    joyRight: { x: 0, y: 0, active: false, id: null },

    // History for CSV export
    history: []
  };

  // --- DOM ELEMENTS ---
  const el = {
    espIpInput: document.getElementById('espIpInput'),
    btnConnect: document.getElementById('btnConnect'),
    connectBtnText: document.getElementById('connectBtnText'),
    btnSimMode: document.getElementById('btnSimMode'),
    simBtnText: document.getElementById('simBtnText'),
    btnAudioToggle: document.getElementById('btnAudioToggle'),
    audioIcon: document.getElementById('audioIcon'),
    connStatusPill: document.getElementById('connStatusPill'),
    statusLed: document.getElementById('statusLed'),
    statusText: document.getElementById('statusText'),
    pingValue: document.getElementById('pingValue'),

    // Telemetry displays
    valDistance: document.getElementById('valDistance'),
    distanceStatus: document.getElementById('distanceStatus'),
    obstacleBlip: document.getElementById('obstacleBlip'),
    valTemp: document.getElementById('valTemp'),
    valHumidity: document.getElementById('valHumidity'),
    thermalGauge: document.getElementById('thermalGauge'),
    thermalState: document.getElementById('thermalState'),
    pirCard: document.getElementById('pirCard'),
    pirStatusText: document.getElementById('pirStatusText'),
    pirRawBadge: document.getElementById('pirRawBadge'),
    svgPirIndicator: document.getElementById('svgPirIndicator'),
    aiScoreText: document.getElementById('aiScoreText'),
    aiCircleScore: document.getElementById('aiCircleScore'),
    aiVerdictLabel: document.getElementById('aiVerdictLabel'),
    currentGaitBadge: document.getElementById('currentGaitBadge'),

    // Servo displays
    angFL1: document.getElementById('angFL1'),
    angFL2: document.getElementById('angFL2'),
    angFR1: document.getElementById('angFR1'),
    angFR2: document.getElementById('angFR2'),
    angRL1: document.getElementById('angRL1'),
    angRL2: document.getElementById('angRL2'),
    angRR1: document.getElementById('angRR1'),
    angRR2: document.getElementById('angRR2'),

    // Robot SVG Bones
    boneFL_hip: document.getElementById('boneFL_hip'),
    boneFL_lift: document.getElementById('boneFL_lift'),
    boneFR_hip: document.getElementById('boneFR_hip'),
    boneFR_lift: document.getElementById('boneFR_lift'),
    boneRL_hip: document.getElementById('boneRL_hip'),
    boneRL_lift: document.getElementById('boneRL_lift'),
    boneRR_hip: document.getElementById('boneRR_hip'),
    boneRR_lift: document.getElementById('boneRR_lift'),
    footFL: document.getElementById('footFL'),
    footFR: document.getElementById('footFR'),
    footRL: document.getElementById('footRL'),
    footRR: document.getElementById('footRR'),

    // Gamepad HUD elements
    padHardwareBadge: document.getElementById('padHardwareBadge'),
    padStatusDot: document.getElementById('padStatusDot'),
    padStatusText: document.getElementById('padStatusText'),
    padInfoCaption: document.getElementById('padInfoCaption'),
    hud_stick_L_cap: document.getElementById('hud_stick_L_cap'),
    hud_stick_R_cap: document.getElementById('hud_stick_R_cap'),
    hud_dpad_up: document.getElementById('hud_dpad_up'),
    hud_dpad_down: document.getElementById('hud_dpad_down'),
    hud_dpad_left: document.getElementById('hud_dpad_left'),
    hud_dpad_right: document.getElementById('hud_dpad_right'),
    hud_btn_A: document.getElementById('hud_btn_A'),
    hud_btn_B: document.getElementById('hud_btn_B'),
    hud_btn_X: document.getElementById('hud_btn_X'),
    hud_btn_Y: document.getElementById('hud_btn_Y'),
    hud_LB: document.getElementById('hud_LB'),
    hud_RB: document.getElementById('hud_RB'),

    // Virtual Joysticks
    joyBaseLeft: document.getElementById('joyBaseLeft'),
    joyNippleLeft: document.getElementById('joyNippleLeft'),
    joyCoordsLeft: document.getElementById('joyCoordsLeft'),
    joyBaseRight: document.getElementById('joyBaseRight'),
    joyNippleRight: document.getElementById('joyNippleRight'),
    joyCoordsRight: document.getElementById('joyCoordsRight'),

    // D-Pad buttons
    btnDpadUp: document.getElementById('btnDpadUp'),
    btnDpadDown: document.getElementById('btnDpadDown'),
    btnDpadLeft: document.getElementById('btnDpadLeft'),
    btnDpadRight: document.getElementById('btnDpadRight'),
    btnDpadStop: document.getElementById('btnDpadStop'),
    speedRange: document.getElementById('speedRange'),
    speedValText: document.getElementById('speedValText'),

    // Action buttons
    btnStanceStand: document.getElementById('btnStanceStand'),
    btnStanceCrouch: document.getElementById('btnStanceCrouch'),
    btnStanceWave: document.getElementById('btnStanceWave'),
    btnStancePatrol: document.getElementById('btnStancePatrol'),
    btnEStop: document.getElementById('btnEStop'),

    // Console
    consoleBody: document.getElementById('consoleBody'),
    btnClearLog: document.getElementById('btnClearLog'),
    btnExportLog: document.getElementById('btnExportLog')
  };

  // ===================================================================
  // 1. WEB AUDIO API SYNTHESIZER (No external audio files needed!)
  // ===================================================================
  let audioCtx = null;

  function initAudio() {
    if (!audioCtx) {
      const AudioContext = window.AudioContext || window.webkitAudioContext;
      if (AudioContext) {
        audioCtx = new AudioContext();
      }
    }
    if (audioCtx && audioCtx.state === 'suspended') {
      audioCtx.resume();
    }
  }

  function playTone(freq, type, duration, gainVal = 0.1) {
    if (!state.audioEnabled) return;
    try {
      initAudio();
      if (!audioCtx) return;
      const osc = audioCtx.createOscillator();
      const gain = audioCtx.createGain();
      osc.type = type;
      osc.frequency.setValueAtTime(freq, audioCtx.currentTime);
      gain.gain.setValueAtTime(gainVal, audioCtx.currentTime);
      gain.gain.exponentialRampToValueAtTime(0.0001, audioCtx.currentTime + duration);
      osc.connect(gain);
      gain.connect(audioCtx.destination);
      osc.start();
      osc.stop(audioCtx.currentTime + duration);
    } catch (e) {
      console.warn('Audio tone error:', e);
    }
  }

  const sfx = {
    click: () => playTone(880, 'sine', 0.08, 0.06),
    action: () => {
      playTone(520, 'triangle', 0.08, 0.08);
      setTimeout(() => playTone(780, 'triangle', 0.1, 0.08), 60);
    },
    sonar: () => playTone(1200, 'sine', 0.15, 0.05),
    pirAlert: () => {
      playTone(950, 'sawtooth', 0.12, 0.15);
      setTimeout(() => playTone(1400, 'sawtooth', 0.18, 0.18), 100);
      setTimeout(() => playTone(1800, 'sawtooth', 0.22, 0.2), 220);
    },
    estop: () => {
      playTone(300, 'sawtooth', 0.3, 0.25);
      setTimeout(() => playTone(200, 'sawtooth', 0.4, 0.3), 150);
    },
    step: () => playTone(160, 'sine', 0.04, 0.03)
  };

  // ===================================================================
  // 2. LOGGING & CONSOLE SYSTEM
  // ===================================================================
  function addLog(msg, type = 'SYS') {
    const timeStr = new Date().toLocaleTimeString('id-ID', { hour12: false });
    const row = document.createElement('div');
    row.className = `log-entry log-${type.toLowerCase()}`;

    let tagClass = 'tag-sys';
    if (type === 'PIR') tagClass = 'tag-pir';
    else if (type === 'CMD') tagClass = 'tag-cmd';
    else if (type === 'WARN') tagClass = 'tag-warn';

    row.innerHTML = `
      <span class="log-time">[${timeStr}]</span>
      <span class="log-tag ${tagClass}">${type}</span>
      <span class="log-msg">${msg}</span>
    `;

    el.consoleBody.appendChild(row);
    el.consoleBody.scrollTop = el.consoleBody.scrollHeight;

    // Record to history
    state.history.push({
      timestamp: new Date().toISOString(),
      command: state.currentCommand,
      distance_cm: state.telemetry.distance,
      temperature_c: state.telemetry.temperature,
      humidity_pct: state.telemetry.humidity,
      pir_state: state.telemetry.pirState,
      safety_status: state.telemetry.safetyStatus,
      anomaly_score: state.telemetry.aiScore,
      anomaly_label: state.telemetry.aiVerdict
    });

    if (state.history.length > 500) {
      state.history.shift();
    }
  }

  // ===================================================================
  // 3. W3C GAMEPAD API & PHYSICAL CONTROLLER INTEGRATION
  // ===================================================================
  window.addEventListener('gamepadconnected', (e) => {
    state.activeGamepads[e.gamepad.index] = e.gamepad;
    state.selectedGamepadIndex = e.gamepad.index;
    el.padStatusDot.classList.add('active');
    el.padStatusText.textContent = `PAD TERHUBUNG: ${e.gamepad.id.slice(0, 22)}...`;
    el.padInfoCaption.innerHTML = `<span style="color:#00ff88;font-weight:700;">Gamepad Terdeteksi!</span> ${e.gamepad.id}`;
    addLog(`Gamepad Fisik Terdeteksi: ${e.gamepad.id} (ID: ${e.gamepad.index})`, 'SYS');
    sfx.action();
    triggerHaptic(e.gamepad, 0.4, 0.6, 200);
  });

  window.addEventListener('gamepaddisconnected', (e) => {
    delete state.activeGamepads[e.gamepad.index];
    const remaining = Object.keys(state.activeGamepads);
    if (remaining.length > 0) {
      state.selectedGamepadIndex = remaining[0];
      el.padStatusText.textContent = `PAD: ${state.activeGamepads[remaining[0]].id.slice(0, 20)}...`;
    } else {
      state.selectedGamepadIndex = null;
      el.padStatusDot.classList.remove('active');
      el.padStatusText.textContent = 'MENCARI GAMEPAD USB/BT...';
      el.padInfoCaption.textContent = 'Hubungkan Gamepad USB/Bluetooth atau gunakan Virtual Touch Stick di bawah!';
    }
    addLog(`Gamepad terputus: Index ${e.gamepad.index}`, 'WARN');
  });

  function triggerHaptic(gp, weak = 0.5, strong = 0.5, duration = 150) {
    if (gp && gp.vibrationActuator && typeof gp.vibrationActuator.playEffect === 'function') {
      try {
        gp.vibrationActuator.playEffect('dual-rumble', {
          startDelay: 0,
          duration: duration,
          weakMagnitude: weak,
          strongMagnitude: strong
        }).catch(() => {});
      } catch (err) {}
    }
  }

  // Polling loop for Gamepad State
  let lastSentCommand = '';
  let lastCommandTime = 0;

  function pollGamepads() {
    const gamepads = navigator.getGamepads ? navigator.getGamepads() : [];
    let activePad = null;

    if (state.selectedGamepadIndex !== null && gamepads[state.selectedGamepadIndex]) {
      activePad = gamepads[state.selectedGamepadIndex];
    } else {
      // Find first non-null gamepad
      for (let i = 0; i < gamepads.length; i++) {
        if (gamepads[i]) {
          activePad = gamepads[i];
          state.selectedGamepadIndex = i;
          break;
        }
      }
    }

    if (activePad) {
      processGamepadInput(activePad);
    }

    requestAnimationFrame(pollGamepads);
  }

  function processGamepadInput(gp) {
    const deadzone = 0.20;

    // Read Left Stick (Axis 0 = X, Axis 1 = Y)
    let lx = gp.axes[0] || 0;
    let ly = gp.axes[1] || 0;
    if (Math.abs(lx) < deadzone) lx = 0;
    if (Math.abs(ly) < deadzone) ly = 0;

    // Read Right Stick (Axis 2 = X, Axis 3 = Y)
    let rx = gp.axes[2] || 0;
    let ry = gp.axes[3] || 0;
    if (Math.abs(rx) < deadzone) rx = 0;
    if (Math.abs(ry) < deadzone) ry = 0;

    // Update HUD Stick Cap displacements
    if (el.hud_stick_L_cap) {
      el.hud_stick_L_cap.setAttribute('cx', (lx * 8).toFixed(1));
      el.hud_stick_L_cap.setAttribute('cy', (ly * 8).toFixed(1));
    }
    if (el.hud_stick_R_cap) {
      el.hud_stick_R_cap.setAttribute('cx', (rx * 8).toFixed(1));
      el.hud_stick_R_cap.setAttribute('cy', (ry * 8).toFixed(1));
    }

    // Read Buttons
    // Standard mapping: 0=A, 1=B, 2=X, 3=Y, 4=LB, 5=RB, 12=DUp, 13=DDown, 14=DLeft, 15=DRight
    const btnA = gp.buttons[0] && gp.buttons[0].pressed;
    const btnB = gp.buttons[1] && gp.buttons[1].pressed;
    const btnX = gp.buttons[2] && gp.buttons[2].pressed;
    const btnY = gp.buttons[3] && gp.buttons[3].pressed;
    const btnLB = gp.buttons[4] && gp.buttons[4].pressed;
    const btnRB = gp.buttons[5] && gp.buttons[5].pressed;
    const dUp = gp.buttons[12] && gp.buttons[12].pressed;
    const dDown = gp.buttons[13] && gp.buttons[13].pressed;
    const dLeft = gp.buttons[14] && gp.buttons[14].pressed;
    const dRight = gp.buttons[15] && gp.buttons[15].pressed;

    // Update SVG HUD Visual Highlights
    updateHudElement(el.hud_btn_A, btnA, '#22c55e');
    updateHudElement(el.hud_btn_B, btnB, '#ef4444');
    updateHudElement(el.hud_btn_X, btnX, '#3b82f6');
    updateHudElement(el.hud_btn_Y, btnY, '#eab308');
    updateHudElement(el.hud_LB, btnLB, '#00f3ff');
    updateHudElement(el.hud_RB, btnRB, '#00f3ff');
    updateHudElement(el.hud_dpad_up, dUp, '#00f3ff');
    updateHudElement(el.hud_dpad_down, dDown, '#00f3ff');
    updateHudElement(el.hud_dpad_left, dLeft, '#00f3ff');
    updateHudElement(el.hud_dpad_right, dRight, '#00f3ff');

    // Interpret Priority Commands
    let targetCmd = null;

    if (btnB) {
      targetCmd = 'STOP';
    } else if (btnA) {
      targetCmd = 'STAND';
    } else if (btnX) {
      targetCmd = 'CROUCH';
    } else if (btnY) {
      targetCmd = 'WAVE';
    } else if (dUp || ly < -0.4) {
      targetCmd = 'FORWARD';
    } else if (dDown || ly > 0.4) {
      targetCmd = 'BACKWARD';
    } else if (dLeft || rx < -0.4 || lx < -0.5) {
      targetCmd = 'LEFT';
    } else if (dRight || rx > 0.4 || lx > 0.5) {
      targetCmd = 'RIGHT';
    }

    // Speed shift with bumpers
    if (btnLB && state.currentSpeed > 1) {
      setSpeed(state.currentSpeed - 1);
    } else if (btnRB && state.currentSpeed < 3) {
      setSpeed(state.currentSpeed + 1);
    }

    if (targetCmd && targetCmd !== state.currentCommand) {
      sendCommand(targetCmd);
      triggerHaptic(gp, 0.3, 0.4, 80);
    } else if (!targetCmd && (state.currentCommand === 'FORWARD' || state.currentCommand === 'BACKWARD' || state.currentCommand === 'LEFT' || state.currentCommand === 'RIGHT')) {
      // Natural return to STOP when stick is released
      sendCommand('STOP');
    }
  }

  function updateHudElement(elem, isActive, activeColor) {
    if (!elem) return;
    if (isActive) {
      elem.setAttribute('fill', activeColor);
      elem.setAttribute('filter', 'drop-shadow(0 0 6px ' + activeColor + ')');
    } else {
      elem.setAttribute('fill', '#1e293b');
      elem.removeAttribute('filter');
    }
  }

  // ===================================================================
  // 4. VIRTUAL TOUCH / MOUSE ANALOG JOYSTICKS
  // ===================================================================
  function setupVirtualJoystick(baseEl, nippleEl, coordsEl, isRight = false) {
    const joyState = isRight ? state.joyRight : state.joyLeft;
    const maxRadius = 38;

    function handlePointer(clientX, clientY) {
      const rect = baseEl.getBoundingClientRect();
      const centerX = rect.left + rect.width / 2;
      const centerY = rect.top + rect.height / 2;
      let dx = clientX - centerX;
      let dy = clientY - centerY;
      const dist = Math.hypot(dx, dy);

      if (dist > maxRadius) {
        dx = (dx / dist) * maxRadius;
        dy = (dy / dist) * maxRadius;
      }

      nippleEl.style.transform = `translate(${dx}px, ${dy}px)`;
      const normX = +(dx / maxRadius).toFixed(2);
      const normY = +(dy / maxRadius).toFixed(2);
      joyState.x = normX;
      joyState.y = normY;
      coordsEl.textContent = `X: ${normX} | Y: ${normY}`;

      // Movement command logic
      if (isRight) {
        // Rotation (Left / Right)
        if (normX < -0.45) sendCommand('LEFT');
        else if (normX > 0.45) sendCommand('RIGHT');
        else if (state.currentCommand === 'LEFT' || state.currentCommand === 'RIGHT') sendCommand('STOP');
      } else {
        // Translation (Forward / Backward)
        if (normY < -0.45) sendCommand('FORWARD');
        else if (normY > 0.45) sendCommand('BACKWARD');
        else if (normX < -0.55) sendCommand('LEFT');
        else if (normX > 0.55) sendCommand('RIGHT');
        else if (state.currentCommand === 'FORWARD' || state.currentCommand === 'BACKWARD') sendCommand('STOP');
      }
    }

    function resetStick() {
      joyState.active = false;
      joyState.x = 0;
      joyState.y = 0;
      nippleEl.style.transform = 'translate(0px, 0px)';
      coordsEl.textContent = 'X: 0.00 | Y: 0.00';
      if (state.currentCommand === 'FORWARD' || state.currentCommand === 'BACKWARD' || state.currentCommand === 'LEFT' || state.currentCommand === 'RIGHT') {
        sendCommand('STOP');
      }
    }

    baseEl.addEventListener('pointerdown', (e) => {
      joyState.active = true;
      joyState.id = e.pointerId;
      baseEl.setPointerCapture(e.pointerId);
      handlePointer(e.clientX, e.clientY);
      sfx.click();
    });

    baseEl.addEventListener('pointermove', (e) => {
      if (joyState.active && e.pointerId === joyState.id) {
        handlePointer(e.clientX, e.clientY);
      }
    });

    baseEl.addEventListener('pointerup', (e) => {
      if (e.pointerId === joyState.id) {
        resetStick();
      }
    });

    baseEl.addEventListener('pointercancel', (e) => {
      if (e.pointerId === joyState.id) {
        resetStick();
      }
    });
  }

  // ===================================================================
  // 5. GAIT KINEMATICS & 8-SERVO POSTURE SIMULATOR
  // ===================================================================
  function updateServoDisplay() {
    el.angFL1.textContent = `${state.servos.flHip}°`;
    el.angFL2.textContent = `${state.servos.flLift}°`;
    el.angFR1.textContent = `${state.servos.frHip}°`;
    el.angFR2.textContent = `${state.servos.frLift}°`;
    el.angRL1.textContent = `${state.servos.rlHip}°`;
    el.angRL2.textContent = `${state.servos.rlLift}°`;
    el.angRR1.textContent = `${state.servos.rrHip}°`;
    el.angRR2.textContent = `${state.servos.rrLift}°`;

    // Visual kinematic bending of SVG legs
    const dFL = (state.servos.flHip - 90) * 0.4;
    const dFR = (state.servos.frHip - 90) * 0.4;
    const dRL = (state.servos.rlHip - 90) * 0.4;
    const dRR = (state.servos.rrHip - 90) * 0.4;

    el.boneFL_lift.setAttribute('x2', 60 + dFL);
    el.footFL.setAttribute('cx', 60 + dFL);

    el.boneFR_lift.setAttribute('x2', 440 + dFR);
    el.footFR.setAttribute('cx', 440 + dFR);

    el.boneRL_lift.setAttribute('x2', 60 + dRL);
    el.footRL.setAttribute('cx', 60 + dRL);

    el.boneRR_lift.setAttribute('x2', 440 + dRR);
    el.footRR.setAttribute('cx', 440 + dRR);
  }

  function startGaitCycle(type) {
    if (state.gaitInterval) clearInterval(state.gaitInterval);

    state.gaitStep = 0;
    const baseInterval = state.currentSpeed === 3 ? 120 : (state.currentSpeed === 2 ? 180 : 250);

    state.gaitInterval = setInterval(() => {
      state.gaitStep = (state.gaitStep + 1) % 4;

      if (type === 'FORWARD') {
        // Trot Gait: Diagonal pairs alternate lift & swing
        if (state.gaitStep === 0) {
          // Pair 1 (FL + RR) Lift and Swing Forward
          state.servos.flLift = 70;
          state.servos.flHip = 105;
          state.servos.rrLift = 70;
          state.servos.rrHip = 105;
          // Pair 2 (FR + RL) Support and Push Back
          state.servos.frLift = 90;
          state.servos.frHip = 75;
          state.servos.rlLift = 90;
          state.servos.rlHip = 75;
        } else if (state.gaitStep === 1) {
          state.servos.flLift = 90;
          state.servos.rrLift = 90;
        } else if (state.gaitStep === 2) {
          // Pair 2 (FR + RL) Lift and Swing Forward
          state.servos.frLift = 70;
          state.servos.frHip = 105;
          state.servos.rlLift = 70;
          state.servos.rlHip = 105;
          // Pair 1 (FL + RR) Support and Push Back
          state.servos.flLift = 90;
          state.servos.flHip = 75;
          state.servos.rrLift = 90;
          state.servos.rrHip = 75;
        } else if (state.gaitStep === 3) {
          state.servos.frLift = 90;
          state.servos.rlLift = 90;
        }
        sfx.step();
      } else if (type === 'BACKWARD') {
        // Inverse trot gait
        if (state.gaitStep === 0) {
          state.servos.flLift = 70;
          state.servos.flHip = 75;
          state.servos.rrLift = 70;
          state.servos.rrHip = 75;
          state.servos.frLift = 90;
          state.servos.frHip = 105;
          state.servos.rlLift = 90;
          state.servos.rlHip = 105;
        } else if (state.gaitStep === 2) {
          state.servos.frLift = 70;
          state.servos.frHip = 75;
          state.servos.rlLift = 70;
          state.servos.rlHip = 75;
          state.servos.flLift = 90;
          state.servos.flHip = 105;
          state.servos.rrLift = 90;
          state.servos.rrHip = 105;
        }
        sfx.step();
      } else if (type === 'LEFT') {
        state.servos.flHip = 80;
        state.servos.frHip = 110;
        state.servos.rlHip = 80;
        state.servos.rrHip = 110;
      } else if (type === 'RIGHT') {
        state.servos.flHip = 110;
        state.servos.frHip = 80;
        state.servos.rlHip = 110;
        state.servos.rrHip = 80;
      } else if (type === 'WAVE') {
        // Wave front right paw
        state.servos.frLift = state.gaitStep % 2 === 0 ? 50 : 30;
        state.servos.frHip = state.gaitStep % 2 === 0 ? 120 : 80;
      }

      updateServoDisplay();
    }, baseInterval);
  }

  function stopGaitCycle() {
    if (state.gaitInterval) {
      clearInterval(state.gaitInterval);
      state.gaitInterval = null;
    }
  }

  // ===================================================================
  // 6. COMMAND DISPATCHER & SAFETY INTERLOCK
  // ===================================================================
  function sendCommand(cmd) {
    // Safety Interlock check: Obstacle avoidance threshold
    if (cmd === 'FORWARD' && state.telemetry.distance <= 10.0) {
      addLog(`KONTROL DITOLAK: Halangan terdeteksi pada ${state.telemetry.distance} cm!`, 'WARN');
      sfx.estop();
      el.distanceStatus.textContent = 'HALANGAN TERLALU DEKAT! MAJU DIBLOKIR';
      el.distanceStatus.classList.add('warning');
      triggerHaptic(state.activeGamepads[state.selectedGamepadIndex], 0.8, 0.9, 300);
      return;
    }

    state.currentCommand = cmd;
    el.currentGaitBadge.textContent = `POSTUR: ${cmd}`;

    if (cmd === 'FORWARD' || cmd === 'BACKWARD' || cmd === 'LEFT' || cmd === 'RIGHT' || cmd === 'WAVE') {
      startGaitCycle(cmd);
    } else {
      stopGaitCycle();
      if (cmd === 'STAND') {
        Object.keys(state.servos).forEach(k => state.servos[k] = 90);
      } else if (cmd === 'CROUCH') {
        state.servos.flLift = 130;
        state.servos.frLift = 130;
        state.servos.rlLift = 130;
        state.servos.rrLift = 130;
      } else if (cmd === 'STOP') {
        // Neutral hold
        Object.keys(state.servos).forEach(k => state.servos[k] = 90);
        sfx.click();
      }
      updateServoDisplay();
    }

    addLog(`Perintah dikirim: [${cmd}] (Kecepatan: ${state.currentSpeed}x)`, 'CMD');

    // If live mode, transmit HTTP GET/POST to ESP32
    if (!state.simMode) {
      transmitToEsp(cmd, state.currentSpeed);
    }
  }

  function setSpeed(speedLevel) {
    state.currentSpeed = speedLevel;
    el.speedRange.value = speedLevel;
    const names = { 1: 'ECO (1x)', 2: 'NORMAL (2x)', 3: 'SPORT (3x)' };
    el.speedValText.textContent = names[speedLevel] || 'NORMAL (2x)';
    addLog(`Kecepatan gait diatur: ${names[speedLevel]}`, 'SYS');
    sfx.click();
  }

  // Transmit command to physical ESP32 via HTTP REST
  async function transmitToEsp(cmd, speed) {
    const url = `http://${state.espIp}/command?cmd=${cmd}&speed=${speed}`;
    try {
      const startT = performance.now();
      const res = await fetch(url, { method: 'GET', mode: 'cors', cache: 'no-store' });
      const ping = Math.round(performance.now() - startT);
      el.pingValue.textContent = `${ping} ms`;
      if (res.ok) {
        const json = await res.json();
        handleTelemetryUpdate(json);
      }
    } catch (err) {
      console.warn('ESP32 transmission error:', err);
      el.statusLed.className = 'indicator-led offline';
      el.statusText.textContent = 'ESP32 OFFLINE';
    }
  }

  // ===================================================================
  // 7. TELEMETRY PROCESSING & SIMULATION ENGINE
  // ===================================================================
  function handleTelemetryUpdate(data) {
    if (data.distance !== undefined) state.telemetry.distance = +data.distance.toFixed(1);
    if (data.temperature !== undefined) state.telemetry.temperature = +data.temperature.toFixed(1);
    if (data.humidity !== undefined) state.telemetry.humidity = +data.humidity.toFixed(1);
    if (data.pir !== undefined) {
      const prev = state.telemetry.pirState;
      state.telemetry.pirState = data.pir ? 1 : 0;
      if (state.telemetry.pirState === 1 && prev === 0) {
        onPirIntruderAlert();
      }
    }

    renderTelemetry();
  }

  function onPirIntruderAlert() {
    el.pirCard.classList.add('motion-detected');
    el.pirStatusText.textContent = 'PERINGATAN: GERAKAN TERDETEKSI!';
    el.pirRawBadge.textContent = '1';
    el.svgPirIndicator.setAttribute('fill', '#ff2a55');
    el.svgPirIndicator.setAttribute('r', '11');

    addLog('SENSOR PIR: Pancaran inframerah terdeteksi! Manusia/objek bergerak di depan robot.', 'PIR');
    sfx.pirAlert();

    // Haptic pulse on gamepad
    triggerHaptic(state.activeGamepads[state.selectedGamepadIndex], 0.7, 0.9, 400);

    // AI Anomaly Spike
    state.telemetry.aiScore = 0.61;
    state.telemetry.aiVerdict = 'PERLU DIPERIKSA';
    el.aiVerdictLabel.textContent = 'ANOMALI: PERLU DIPERIKSA';
    el.aiVerdictLabel.style.color = 'var(--neon-amber)';
    el.aiCircleScore.style.stroke = 'var(--neon-amber)';
    el.aiCircleScore.setAttribute('stroke-dasharray', '61, 100');
    el.aiScoreText.textContent = '0.61';

    // Auto reset PIR visual after 5 seconds if clear
    setTimeout(() => {
      if (state.telemetry.pirState === 0) {
        resetPirVisual();
      }
    }, 5000);
  }

  function resetPirVisual() {
    el.pirCard.classList.remove('motion-detected');
    el.pirStatusText.textContent = 'STANDBY // AREA AMAN';
    el.pirRawBadge.textContent = '0';
    el.svgPirIndicator.setAttribute('fill', '#ff2a55');
    el.svgPirIndicator.setAttribute('r', '8');

    state.telemetry.aiScore = 0.94;
    state.telemetry.aiVerdict = 'NORMAL';
    el.aiVerdictLabel.textContent = 'POLA DATA NORMAL';
    el.aiVerdictLabel.style.color = 'var(--neon-green)';
    el.aiCircleScore.style.stroke = 'var(--neon-green)';
    el.aiCircleScore.setAttribute('stroke-dasharray', '94, 100');
    el.aiScoreText.textContent = '0.94';
  }

  function renderTelemetry() {
    // Distance
    el.valDistance.textContent = state.telemetry.distance.toFixed(1);
    if (state.telemetry.distance <= 10.0) {
      el.distanceStatus.textContent = 'HALANGAN DEKAT (< 10 cm)! STOP AKTIF';
      el.distanceStatus.className = 'metric-status warning';
      el.obstacleBlip.setAttribute('cx', '100');
      el.obstacleBlip.setAttribute('cy', '100');
    } else {
      el.distanceStatus.textContent = 'JARAK AMAN (> 10 cm)';
      el.distanceStatus.className = 'metric-status';
      const arcY = Math.max(20, Math.min(100, 110 - (state.telemetry.distance * 1.5)));
      el.obstacleBlip.setAttribute('cy', arcY.toFixed(0));
    }

    // Climate
    el.valTemp.textContent = state.telemetry.temperature.toFixed(1);
    el.valHumidity.textContent = state.telemetry.humidity.toFixed(1);
    const tempRatio = Math.min(100, Math.max(0, (state.telemetry.temperature - 20) * 5));
    el.thermalGauge.style.width = `${tempRatio}%`;
    el.thermalState.textContent = state.telemetry.temperature > 35 ? 'Panas' : 'Optimal';
  }

  // Simulation Loop (runs every 1.5s when simMode is active)
  let simTimer = null;
  function startSimulation() {
    if (simTimer) clearInterval(simTimer);
    simTimer = setInterval(() => {
      if (!state.simMode) return;

      // Distance subtle drift
      let d = state.telemetry.distance + (Math.random() * 4 - 2);
      if (d < 12) d = 35 + Math.random() * 20;
      if (d > 80) d = 75;
      state.telemetry.distance = +d.toFixed(1);

      // Climate subtle drift
      state.telemetry.temperature = +(28.0 + (Math.random() * 1.5)).toFixed(1);
      state.telemetry.humidity = +(64.0 + (Math.random() * 3)).toFixed(1);

      // Random PIR motion event (1 in 15 chance)
      if (Math.random() < 0.08 && state.telemetry.pirState === 0) {
        state.telemetry.pirState = 1;
        onPirIntruderAlert();
        setTimeout(() => {
          state.telemetry.pirState = 0;
          resetPirVisual();
        }, 4000);
      }

      renderTelemetry();
    }, 1500);
  }

  // ===================================================================
  // 8. KEYBOARD SHORTCUTS
  // ===================================================================
  const activeKeys = new Set();

  window.addEventListener('keydown', (e) => {
    // Avoid interfering with IP input typing
    if (document.activeElement === el.espIpInput) return;

    const key = e.key.toUpperCase();
    if (activeKeys.has(key)) return;
    activeKeys.add(key);

    if (key === 'W' || key === 'ARROWUP') {
      sendCommand('FORWARD');
      el.btnDpadUp.classList.add('pressed');
    } else if (key === 'S' || key === 'ARROWDOWN') {
      sendCommand('BACKWARD');
      el.btnDpadDown.classList.add('pressed');
    } else if (key === 'A' || key === 'ARROWLEFT') {
      sendCommand('LEFT');
      el.btnDpadLeft.classList.add('pressed');
    } else if (key === 'D' || key === 'ARROWRIGHT') {
      sendCommand('RIGHT');
      el.btnDpadRight.classList.add('pressed');
    } else if (key === ' ' || key === 'ESCAPE') {
      sendCommand('STOP');
      el.btnDpadStop.classList.add('pressed');
      sfx.estop();
    } else if (key === 'X') {
      sendCommand('STAND');
    } else if (key === 'C') {
      sendCommand('CROUCH');
    } else if (key === '1') setSpeed(1);
    else if (key === '2') setSpeed(2);
    else if (key === '3') setSpeed(3);
  });

  window.addEventListener('keyup', (e) => {
    const key = e.key.toUpperCase();
    activeKeys.delete(key);

    if (key === 'W' || key === 'ARROWUP') el.btnDpadUp.classList.remove('pressed');
    if (key === 'S' || key === 'ARROWDOWN') el.btnDpadDown.classList.remove('pressed');
    if (key === 'A' || key === 'ARROWLEFT') el.btnDpadLeft.classList.remove('pressed');
    if (key === 'D' || key === 'ARROWRIGHT') el.btnDpadRight.classList.remove('pressed');
    if (key === ' ' || key === 'ESCAPE') el.btnDpadStop.classList.remove('pressed');

    // If no movement keys are held down, return to STOP
    const movementKeys = ['W', 'S', 'A', 'D', 'ARROWUP', 'ARROWDOWN', 'ARROWLEFT', 'ARROWRIGHT'];
    const hasMovement = movementKeys.some(k => activeKeys.has(k));
    if (!hasMovement && (state.currentCommand === 'FORWARD' || state.currentCommand === 'BACKWARD' || state.currentCommand === 'LEFT' || state.currentCommand === 'RIGHT')) {
      sendCommand('STOP');
    }
  });

  // ===================================================================
  // 9. EVENT LISTENERS & UI WIRING
  // ===================================================================
  function initListeners() {
    // Sim Mode Toggle
    el.btnSimMode.addEventListener('click', () => {
      state.simMode = !state.simMode;
      if (state.simMode) {
        el.btnSimMode.classList.add('active');
        el.statusLed.className = 'indicator-led online';
        el.statusText.textContent = 'SIMULASI AKTIF';
        addLog('Mode Simulasi Diaktifkan.', 'SYS');
        startSimulation();
      } else {
        el.btnSimMode.classList.remove('active');
        el.statusText.textContent = 'MODE REAL TIME';
        addLog(`Mode Nyata Diaktifkan. Host: ${state.espIp}`, 'SYS');
      }
      sfx.click();
    });

    // Audio FX Toggle
    el.btnAudioToggle.addEventListener('click', () => {
      state.audioEnabled = !state.audioEnabled;
      el.audioIcon.textContent = state.audioEnabled ? '🔊' : '🔇';
      addLog(`Efek Suara: ${state.audioEnabled ? 'Aktif' : 'Nonaktif'}`, 'SYS');
      if (state.audioEnabled) sfx.action();
    });

    // Wiring Diagram Modal Viewer
    const btnOpenWiring = document.getElementById('btnOpenWiring');
    const btnCloseWiring = document.getElementById('btnCloseWiring');
    const wiringModal = document.getElementById('wiringModal');

    if (btnOpenWiring && wiringModal) {
      btnOpenWiring.addEventListener('click', () => {
        wiringModal.style.display = 'flex';
        addLog('Membuka Skema Wiring & Pengkabelan Lengkap.', 'SYS');
        sfx.action();
      });
    }

    if (btnCloseWiring && wiringModal) {
      btnCloseWiring.addEventListener('click', () => {
        wiringModal.style.display = 'none';
        sfx.click();
      });
      wiringModal.addEventListener('click', (e) => {
        if (e.target === wiringModal) {
          wiringModal.style.display = 'none';
          sfx.click();
        }
      });
    }

    // Host IP Connect
    el.btnConnect.addEventListener('click', () => {
      const ip = el.espIpInput.value.trim();
      if (ip) {
        state.espIp = ip;
        state.simMode = false;
        el.btnSimMode.classList.remove('active');
        el.statusText.textContent = `MENGHUBUNGKAN: ${ip}...`;
        addLog(`Menghubungkan ke ESP32 pada http://${ip}...`, 'SYS');
        sfx.action();
        transmitToEsp('STAND', state.currentSpeed);
      }
    });

    // Stance Buttons
    el.btnStanceStand.addEventListener('click', () => { sendCommand('STAND'); sfx.action(); });
    el.btnStanceCrouch.addEventListener('click', () => { sendCommand('CROUCH'); sfx.action(); });
    el.btnStanceWave.addEventListener('click', () => { sendCommand('WAVE'); sfx.action(); });
    el.btnStancePatrol.addEventListener('click', () => {
      addLog('MODUL PATROLI OTOMATIS: Robot menyusuri perimeter dengan sensor PIR aktif.', 'SYS');
      sendCommand('FORWARD');
      sfx.action();
    });

    // E-STOP Button
    el.btnEStop.addEventListener('click', () => {
      sendCommand('STOP');
      sfx.estop();
      addLog('EMERGENCY STOP DIAKTIFKAN OLEH OPERATOR!', 'WARN');
      triggerHaptic(state.activeGamepads[state.selectedGamepadIndex], 1.0, 1.0, 500);
    });

    // D-Pad Touch/Click
    el.btnDpadUp.addEventListener('click', () => sendCommand('FORWARD'));
    el.btnDpadDown.addEventListener('click', () => sendCommand('BACKWARD'));
    el.btnDpadLeft.addEventListener('click', () => sendCommand('LEFT'));
    el.btnDpadRight.addEventListener('click', () => sendCommand('RIGHT'));
    el.btnDpadStop.addEventListener('click', () => sendCommand('STOP'));

    // Speed Slider
    el.speedRange.addEventListener('input', (e) => {
      setSpeed(parseInt(e.target.value, 10));
    });

    // PIR Card Click trigger (for quick manual testing of intruder alert!)
    el.pirCard.addEventListener('click', () => {
      if (state.telemetry.pirState === 0) {
        state.telemetry.pirState = 1;
        onPirIntruderAlert();
        setTimeout(() => {
          state.telemetry.pirState = 0;
          resetPirVisual();
        }, 4000);
      }
    });

    // Console Log Buttons
    el.btnClearLog.addEventListener('click', () => {
      el.consoleBody.innerHTML = '';
      addLog('Log konsol dibersihkan.', 'SYS');
      sfx.click();
    });

    el.btnExportLog.addEventListener('click', exportCsv);

    // Sidebar Toggle
    const sidebar = document.getElementById('sidebar');
    const sidebarToggle = document.getElementById('sidebarToggle');
    if (sidebarToggle && sidebar) {
      sidebarToggle.addEventListener('click', () => {
        sidebar.classList.toggle('collapsed');
        sfx.click();
      });
    }

    // Navigation Pills Smooth Scrolling & Active State
    const navPills = document.querySelectorAll('.sidebar-nav .nav-pill');
    navPills.forEach(pill => {
      pill.addEventListener('click', (e) => {
        if (pill.id === 'btnOpenWiring') return; // Handled separately
        e.preventDefault();
        navPills.forEach(p => p.classList.remove('active'));
        pill.classList.add('active');

        const navTarget = pill.getAttribute('data-nav');
        const targetMap = {
          'dashboard': 'secHero',
          'sensors': 'secMiddle',
          'control': 'secRobot',
          'gamepad': 'secGamepad',
          'logs': 'secLogs'
        };

        const targetEl = document.getElementById(targetMap[navTarget]);
        if (targetEl) {
          targetEl.scrollIntoView({ behavior: 'smooth', block: 'start' });
        }
        sfx.click();
      });
    });

    // Test PIR Alert Button
    const btnTestPirAlert = document.getElementById('btnTestPirAlert');
    if (btnTestPirAlert) {
      btnTestPirAlert.addEventListener('click', (e) => {
        e.stopPropagation();
        if (state.telemetry.pirState === 0) {
          state.telemetry.pirState = 1;
          onPirIntruderAlert();
          setTimeout(() => {
            state.telemetry.pirState = 0;
            resetPirVisual();
          }, 4500);
        }
      });
    }

    // Global Command Search Input
    const globalSearchInput = document.getElementById('globalSearchInput');
    if (globalSearchInput) {
      globalSearchInput.addEventListener('keydown', (e) => {
        if (e.key === 'Enter') {
          const query = globalSearchInput.value.trim().toUpperCase();
          if (query === 'STAND' || query === 'BERDIRI') { sendCommand('STAND'); sfx.action(); }
          else if (query === 'CROUCH' || query === 'MERUNDUK') { sendCommand('CROUCH'); sfx.action(); }
          else if (query === 'WAVE') { sendCommand('WAVE'); sfx.action(); }
          else if (query === 'PATROL' || query === 'PATROLI') { sendCommand('FORWARD'); sfx.action(); }
          else if (query === 'STOP' || query === 'ESTOP') { sendCommand('STOP'); sfx.estop(); }
          else if (query === 'WIRING') { document.getElementById('wiringModal').style.display = 'flex'; }
          else {
            addLog(`Pencarian perintah: [${query}] - Coba STAND, CROUCH, WAVE, PATROL, STOP`, 'SYS');
          }
          globalSearchInput.value = '';
        }
      });

      // Keyboard shortcut ⌘K / Ctrl+K
      window.addEventListener('keydown', (e) => {
        if ((e.metaKey || e.ctrlKey) && e.key === 'k') {
          e.preventDefault();
          globalSearchInput.focus();
        }
      });
    }

    // Setup Dual Virtual Joysticks
    setupVirtualJoystick(el.joyBaseLeft, el.joyNippleLeft, el.joyCoordsLeft, false);
    setupVirtualJoystick(el.joyBaseRight, el.joyNippleRight, el.joyCoordsRight, true);
  }

  // ===================================================================
  // 10. CSV DATA EXPORT FOR AI LOGGING
  // ===================================================================
  function exportCsv() {
    if (state.history.length === 0) {
      alert('Belum ada data riwayat telemetri untuk diekspor!');
      return;
    }

    const headers = [
      'timestamp',
      'command',
      'distance_cm',
      'temperature_c',
      'humidity_pct',
      'pir_state',
      'safety_status',
      'anomaly_score',
      'anomaly_label'
    ];

    const rows = state.history.map(item => [
      item.timestamp,
      item.command,
      item.distance_cm,
      item.temperature_c,
      item.humidity_pct,
      item.pir_state,
      item.safety_status,
      item.anomaly_score,
      item.anomaly_label
    ]);

    const csvContent = 'data:text/csv;charset=utf-8,' +
      [headers.join(','), ...rows.map(e => e.join(','))].join('\n');

    const encodedUri = encodeURI(csvContent);
    const link = document.createElement('a');
    link.setAttribute('href', encodedUri);
    link.setAttribute('download', `vortex_x_telemetry_${Date.now()}.csv`);
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);

    addLog(`Data CSV (${state.history.length} baris) berhasil diekspor.`, 'SYS');
    sfx.action();
  }

  // ===================================================================
  // INITIALIZATION ON LOAD
  // ===================================================================
  window.addEventListener('DOMContentLoaded', () => {
    initListeners();
    updateServoDisplay();
    startSimulation();
    requestAnimationFrame(pollGamepads);
    addLog('Gamepad API Listener Aktif. Gunakan stik USB/Bluetooth atau Virtual Joystick.', 'SYS');
  });

})();
