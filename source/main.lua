import "CoreLibs/graphics"
import "CoreLibs/timer"
import "CoreLibs/ui"

local pd <const> = playdate
local gfx <const> = pd.graphics
local snd <const> = pd.sound
-- local DEBUG_ENABLED <const> = true
-- =====================
-- Constants
-- =====================
local SCREEN_W <const> = 400
local SCREEN_H <const> = 240
local SCREEN_CENTER_X <const> = SCREEN_W / 2

local CIRCLE <const> = 360
local MS_PER_SECOND <const> = 1000
local SECONDS_PER_MINUTE <const> = 60

local REFRESH_RATE <const> = 30
local TRANSITION_MS <const> = 300
local PHASE_ALERT_MS <const> = 2000
local SETTINGS_MESSAGE_MS <const> = 1200
local A_LONG_PRESS_MS <const> = 1000
local BLINK_PERIOD_MS <const> = 1300
local BLINK_DUTY <const> = 0.65

local DEFAULT_LOOP_COUNT <const> = 3
local DEFAULT_WORK_MINUTES <const> = 25
local DEFAULT_BREAK_MINUTES <const> = 5

local LOOP_MIN <const> = 1
local LOOP_MAX <const> = 10
local MINUTES_MIN <const> = 1
local MINUTES_MAX <const> = 60

local MODE_CLOCK <const> = "CLOCK"
local MODE_WORKTIME <const> = "WORKTIME"
local MODE_BREAKTIME <const> = "BREAKTIME"
local MODE_SETTINGS <const> = "SETTINGS"
local MODE_THEMES <const> = "THEMES"

local THEME_DARK <const> = "DARK"
local THEME_LIGHT <const> = "LIGHT"

local DIR_UP <const> = "UP"
local DIR_DOWN <const> = "DOWN"
local DIR_LEFT <const> = "LEFT"
local DIR_RIGHT <const> = "RIGHT"

local SETTING_LOOP_INDEX <const> = 1
local SETTING_WORK_INDEX <const> = 2
local SETTING_BREAK_INDEX <const> = 3

local WEEKDAYS <const> = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"}
local SETTINGS_LABELS <const> = {"LOOP", "WORK", "BREAK"}

-- =====================
-- State
-- =====================
local mode = MODE_CLOCK
local activeTimer = nil
local theme = THEME_DARK

local timerResetCrankAccum = 0
local settingsCrankAccum = 0

local loopCount = DEFAULT_LOOP_COUNT
local workMinutes = DEFAULT_WORK_MINUTES
local breakMinutes = DEFAULT_BREAK_MINUTES

local settingsIndex = SETTING_LOOP_INDEX
local settingsAHoldStart = nil
local settingsLongPressDone = false
local settingsMessage = nil
local settingsMessageUntil = 0

local loopRunning = false
local currentLoop = 1
local loopPhase = MODE_WORKTIME

local phaseAlertMessage = nil
local phaseAlertUntil = 0
local phaseAlertNext = nil

local transition = nil
local autoLockDisabled = false

-- 切换动画变量
local transitionFromImage = nil
local transitionToImage = nil
-- forward declaration
local makeModeImage

-- =====================
-- clock cache
-- =====================
local cachedClockSecond = nil
local cachedClockTimeText = ""
local cachedClockDateText = ""

-- =====================
-- Pad layout constants
-- =====================
local PAD_UP_POINTS <const> = {
    38, 35,
    30, 27,
    30, 14,
    46, 14,
    46, 27
}

local PAD_DOWN_POINTS <const> = {
    38, 41,
    46, 49,
    46, 62,
    30, 62,
    30, 49
}

local PAD_LEFT_POINTS <const> = {
    35, 38,
    27, 46,
    14, 46,
    14, 30,
    27, 30
}

local PAD_RIGHT_POINTS <const> = {
    41, 38,
    49, 30,
    62, 30,
    62, 46,
    49, 46
}

local PAD_UP_ARROW <const> = {
    38, 18,
    35, 24,
    41, 24
}

local PAD_DOWN_ARROW <const> = {
    35, 52,
    41, 52,
    38, 58
}

local PAD_LEFT_ARROW <const> = {
    18, 38,
    24, 35,
    24, 41
}

local PAD_RIGHT_ARROW <const> = {
    58, 38,
    52, 35,
    52, 41
}

-- =====================
-- Fonts
-- =====================
local defaultFont = gfx.getSystemFont(gfx.font.kVariantBold)
local timeFont = gfx.font.new("font/Pixel-font/PixelFont") or defaultFont
local timerFont = gfx.font.new("font/Pixel-font/PixelFont-60") or defaultFont

-- =====================
-- Sound
-- =====================
local promptSfx, promptError = snd.sampleplayer.new("sounds/prompt_tone")
if not promptSfx then
    print("sound load failed: " .. tostring(promptError))
end

-- =====================
-- Timer
-- =====================
local workTimer = nil
local breakTimer = nil
local modeTimerMap = {}

local function setTimerAutoLock(disabled)
    if autoLockDisabled == disabled then
        return
    end

    autoLockDisabled = disabled

    if pd.setAutoLockDisabled then
        pd.setAutoLockDisabled(disabled)
    end
end

local function makeTimer(minutes)
    local timer = pd.timer.new(minutes * SECONDS_PER_MINUTE * MS_PER_SECOND)
    timer.discardOnCompletion = false
    timer:pause()
    return timer
end

local function retireTimer(timer)
    if not timer then
        return
    end

    timer:pause()
    timer:reset()

    if timer.remove then
        timer:remove()
    end
end

local function rebuildTimers()
    retireTimer(workTimer)
    retireTimer(breakTimer)

    workTimer = makeTimer(workMinutes)
    breakTimer = makeTimer(breakMinutes)

    modeTimerMap[MODE_WORKTIME] = workTimer
    modeTimerMap[MODE_BREAKTIME] = breakTimer

    activeTimer = nil
end

-- =====================
-- General helpers
-- =====================
local function clamp(value, minValue, maxValue)
    return math.max(minValue, math.min(maxValue, value))
end

local function blinkVisible(period, duty)
    local t = pd.getCurrentTimeMilliseconds() % period
    return t < (period * duty)
end

local function showSettingsMessage(text)
    settingsMessage = text
    settingsMessageUntil = pd.getCurrentTimeMilliseconds() + SETTINGS_MESSAGE_MS
end

local function cancelSettingsHold()
    settingsAHoldStart = nil
    settingsLongPressDone = false
end

local function clearPhaseAlert()
    phaseAlertMessage = nil
    phaseAlertUntil = 0
    phaseAlertNext = nil
end

local function showPhaseAlert(message, afterAlert)
    local now = pd.getCurrentTimeMilliseconds()
    phaseAlertMessage = message
    phaseAlertUntil = now + PHASE_ALERT_MS
    phaseAlertNext = afterAlert

    if promptSfx then
        promptSfx:play()
    end
end

local function finishPhaseAlertIfNeeded(now)
    if not phaseAlertMessage or now < phaseAlertUntil then
        return
    end

    local nextAction = phaseAlertNext
    clearPhaseAlert()

    if nextAction then
        nextAction()
    end
end

local function directionToOffset(direction)
    if direction == DIR_UP then
        return 0, SCREEN_H
    elseif direction == DIR_DOWN then
        return 0, -SCREEN_H
    elseif direction == DIR_LEFT then
        return SCREEN_W, 0
    elseif direction == DIR_RIGHT then
        return -SCREEN_W, 0
    end

    return 0, 0
end

local function startTransition(toMode, direction)
    if mode == toMode then
        return
    end

    local dx, dy = directionToOffset(direction)

    if dx == 0 and dy == 0 then
        mode = toMode
        transition = nil
        return
    end

    transitionFromImage = makeModeImage(mode)

    local oldMode = mode
    mode = toMode
    transitionToImage = makeModeImage(toMode)

    transition = {
        from = oldMode,
        to = toMode,
        dx = dx,
        dy = dy,
        start = pd.getCurrentTimeMilliseconds()
    }
end

local function updateTransition(now)
    if transition and now - transition.start >= TRANSITION_MS then
        transition = nil
        transitionFromImage = nil
        transitionToImage = nil
    end
end

local function getReturnDirection(fromMode)
    if fromMode == MODE_WORKTIME then
        return DIR_DOWN
    elseif fromMode == MODE_BREAKTIME then
        return DIR_UP
    elseif fromMode == MODE_SETTINGS then
        return DIR_RIGHT
    elseif fromMode == MODE_THEMES then
        return DIR_LEFT
    end

    return nil
end

local function stopAllTimers()
    loopRunning = false
    currentLoop = 1
    loopPhase = MODE_WORKTIME
    transition = nil
    clearPhaseAlert()

    if workTimer then
        workTimer:reset()
        workTimer:pause()
    end

    if breakTimer then
        breakTimer:reset()
        breakTimer:pause()
    end

    activeTimer = nil
    setTimerAutoLock(false)
end

local function resetSettingsToDefault()
    loopCount = DEFAULT_LOOP_COUNT
    workMinutes = DEFAULT_WORK_MINUTES
    breakMinutes = DEFAULT_BREAK_MINUTES
    settingsCrankAccum = 0
    timerResetCrankAccum = 0

    stopAllTimers()
    rebuildTimers()
    showSettingsMessage("Defaults reset")
end

local function adjustSetting(delta)
    local oldLoopCount = loopCount
    local oldWorkMinutes = workMinutes
    local oldBreakMinutes = breakMinutes

    if settingsIndex == SETTING_LOOP_INDEX then
        loopCount = clamp(loopCount + delta, LOOP_MIN, LOOP_MAX)
    elseif settingsIndex == SETTING_WORK_INDEX then
        workMinutes = clamp(workMinutes + delta, MINUTES_MIN, MINUTES_MAX)
    elseif settingsIndex == SETTING_BREAK_INDEX then
        breakMinutes = clamp(breakMinutes + delta, MINUTES_MIN, MINUTES_MAX)
    end

    if loopCount ~= oldLoopCount
    or workMinutes ~= oldWorkMinutes
    or breakMinutes ~= oldBreakMinutes then
        stopAllTimers()
        rebuildTimers()
    end
end

local function handleSettingsCrank(change)
    settingsCrankAccum = settingsCrankAccum + change

    while settingsCrankAccum >= CIRCLE do
        adjustSetting(1)
        settingsCrankAccum = settingsCrankAccum - CIRCLE
    end

    while settingsCrankAccum <= -CIRCLE do
        adjustSetting(-1)
        settingsCrankAccum = settingsCrankAccum + CIRCLE
    end
end

local function startOnly(timer)
    if timer == activeTimer and not timer.paused and timer.timeLeft > 0 then
        return
    end

    loopRunning = false
    currentLoop = 1
    loopPhase = mode

    if activeTimer and activeTimer ~= timer then
        activeTimer:pause()
        activeTimer:reset()
    end

    activeTimer = timer
    setTimerAutoLock(true)

    if timer.paused or timer.timeLeft <= 0 then
        timer:reset()
        timer:start()
    end
end

local function startTimer()
    local timer = modeTimerMap[mode]
    if timer then
        startOnly(timer)
    end
end

local function startLoopPhase(phase)
    local timer = modeTimerMap[phase]
    if not timer then
        return
    end

    loopPhase = phase
    mode = phase

    if activeTimer and activeTimer ~= timer then
        activeTimer:pause()
        activeTimer:reset()
    end

    activeTimer = timer
    setTimerAutoLock(true)
    activeTimer:reset()
    activeTimer:start()
end

local function startLoop()
    rebuildTimers()
    loopRunning = true
    currentLoop = 1
    startLoopPhase(MODE_WORKTIME)
end

local function finishLoop()
    stopAllTimers()
    mode = MODE_CLOCK
end

local function stopFinishedTimer()
    if activeTimer then
        activeTimer:pause()
        activeTimer:reset()
    end

    activeTimer = nil

    if not loopRunning then
        setTimerAutoLock(false)
    end
end

local function showLoopPhaseCompleteAlert()
    local completedPhase = loopPhase
    stopFinishedTimer()

    if completedPhase == MODE_WORKTIME then
        showPhaseAlert("WORK DONE", function()
            if loopRunning then
                startLoopPhase(MODE_BREAKTIME)
            end
        end)
        return
    end

    if completedPhase == MODE_BREAKTIME then
        if currentLoop >= loopCount then
            showPhaseAlert("FINISHED", function()
                finishLoop()
            end)
        else
            showPhaseAlert("BREAK DONE", function()
                if loopRunning then
                    currentLoop = currentLoop + 1
                    startLoopPhase(MODE_WORKTIME)
                end
            end)
        end
    end
end

local function showSingleTimerCompleteAlert()
    local completedMode = mode
    stopFinishedTimer()

    if completedMode == MODE_WORKTIME then
        showPhaseAlert("WORK DONE", nil)
    elseif completedMode == MODE_BREAKTIME then
        showPhaseAlert("BREAK DONE", nil)
    else
        showPhaseAlert("DONE", nil)
    end
end

-- =====================
-- Input
-- =====================
local inputState = {}
local function readInput()
    inputState.btnA = pd.buttonJustPressed(pd.kButtonA)
    inputState.btnAReleased = pd.buttonJustReleased(pd.kButtonA)
    inputState.btnAHeld = pd.buttonIsPressed(pd.kButtonA)
    inputState.btnB = pd.buttonJustPressed(pd.kButtonB)
    inputState.up = pd.buttonJustPressed(pd.kButtonUp)
    inputState.down = pd.buttonJustPressed(pd.kButtonDown)
    inputState.left = pd.buttonJustPressed(pd.kButtonLeft)
    inputState.right = pd.buttonJustPressed(pd.kButtonRight)
    inputState.crankChange = pd.getCrankChange()
    return inputState
end

local function handleClockInput(input)
    if mode ~= MODE_CLOCK then
        return false
    end

    local targetMode = nil
    local direction = nil

    if input.up then
        targetMode = loopRunning and loopPhase or MODE_WORKTIME
        direction = DIR_UP
    elseif input.down then
        targetMode = loopRunning and loopPhase or MODE_BREAKTIME
        direction = DIR_DOWN
    elseif input.left then
        targetMode = MODE_SETTINGS
        direction = DIR_LEFT
    elseif input.right then
        targetMode = MODE_THEMES
        direction = DIR_RIGHT
    end

    if targetMode then
        startTransition(targetMode, direction)
        return true
    end

    return false
end

local function handleSettingsSelection(input)
    if mode ~= MODE_SETTINGS then
        return
    end

    if input.up then
        settingsIndex = clamp(settingsIndex - 1, 1, #SETTINGS_LABELS)
        settingsCrankAccum = 0
    elseif input.down then
        settingsIndex = clamp(settingsIndex + 1, 1, #SETTINGS_LABELS)
        settingsCrankAccum = 0
    end
end

local function handleReturnInput(input)
    if mode == MODE_CLOCK then
        return false
    end

    if input.btnB then
        startTransition(MODE_CLOCK, getReturnDirection(mode))
        cancelSettingsHold()
        return true
    end

    if mode == MODE_WORKTIME and input.down then
        startTransition(MODE_CLOCK, DIR_DOWN)
    elseif mode == MODE_BREAKTIME and input.up then
        startTransition(MODE_CLOCK, DIR_UP)
    elseif mode == MODE_SETTINGS and input.right then
        startTransition(MODE_CLOCK, DIR_RIGHT)
    elseif mode == MODE_THEMES and input.left then
        startTransition(MODE_CLOCK, DIR_LEFT)
    else
        return false
    end

    cancelSettingsHold()
    return true
end

local function handleSettingsInput(input, now)
    handleSettingsCrank(input.crankChange)

    if input.btnA then
        settingsAHoldStart = now
        settingsLongPressDone = false
    end

    if input.btnAHeld
    and settingsAHoldStart
    and not settingsLongPressDone
    and now - settingsAHoldStart >= A_LONG_PRESS_MS then
        resetSettingsToDefault()
        settingsLongPressDone = true
    end

    if input.btnAReleased and settingsAHoldStart then
        if not settingsLongPressDone then
            startLoop()
        end

        cancelSettingsHold()
    end
end

local function handleThemeInput(input)
    if mode == MODE_THEMES and input.btnA then
        theme = (theme == THEME_LIGHT) and THEME_DARK or THEME_LIGHT
    end
end

local function resetActiveTimerByCrank(input)
    if mode ~= MODE_WORKTIME and mode ~= MODE_BREAKTIME then
        timerResetCrankAccum = 0
        return
    end

    if input.crankChange == 0 then
        return
    end

    timerResetCrankAccum = timerResetCrankAccum + math.abs(input.crankChange)

    while timerResetCrankAccum >= CIRCLE do
        timerResetCrankAccum = timerResetCrankAccum - CIRCLE

        if activeTimer then
            activeTimer:reset()
            activeTimer:pause()
            activeTimer = nil
            loopRunning = false
            clearPhaseAlert()
            setTimerAutoLock(false)
        end
    end
end

local function handleTimerInput(input)
    if input.btnA then
        startTimer()
    end

    resetActiveTimerByCrank(input)
end

local function handleInput()
    local input = readInput()
    local now = pd.getCurrentTimeMilliseconds()

    if phaseAlertMessage or transition then
        cancelSettingsHold()
        return
    end

    if handleClockInput(input) then
        return
    end

    handleSettingsSelection(input)

    if handleReturnInput(input) then
        return
    end

    if mode == MODE_SETTINGS then
        handleSettingsInput(input, now)
        return
    end

    if mode == MODE_THEMES then
        handleThemeInput(input)
        return
    end

    handleTimerInput(input)
end

-- =====================
-- Timer update
-- =====================
local function updateTimer()
    pd.timer.updateTimers()

    local now = pd.getCurrentTimeMilliseconds()
    updateTransition(now)
    finishPhaseAlertIfNeeded(now)

    if phaseAlertMessage then
        return
    end

    if activeTimer and activeTimer.timeLeft <= 0 then
        if loopRunning then
            showLoopPhaseCompleteAlert()
        else
            showSingleTimerCompleteAlert()
        end
    end
end

-- =====================
-- UI helpers
-- =====================
local function useNormalTextMode()
    if theme == THEME_DARK then
        gfx.setImageDrawMode(gfx.kDrawModeInverted)
    else
        gfx.setImageDrawMode(gfx.kDrawModeCopy)
    end
end

local function useSelectedTextMode()
    if theme == THEME_DARK then
        gfx.setImageDrawMode(gfx.kDrawModeCopy)
    else
        gfx.setImageDrawMode(gfx.kDrawModeInverted)
    end
end

local function getDPadColors(active)
    if theme == THEME_LIGHT then
        if active then
            return gfx.kColorBlack, gfx.kColorBlack, gfx.kColorWhite
        else
            return gfx.kColorWhite, gfx.kColorBlack, gfx.kColorBlack
        end
    end

    if active then
        return gfx.kColorWhite, gfx.kColorWhite, gfx.kColorBlack
    else
        return gfx.kColorBlack, gfx.kColorWhite, gfx.kColorWhite
    end
end

local function drawClosedPolygon(points)
    gfx.drawLine(points[1], points[2], points[3], points[4])
    gfx.drawLine(points[3], points[4], points[5], points[6])
    gfx.drawLine(points[5], points[6], points[7], points[8])
    gfx.drawLine(points[7], points[8], points[9], points[10])
    gfx.drawLine(points[9], points[10], points[1], points[2])
end

local function drawDPadButton(points, active, arrow)
    local fillColor, borderColor, arrowColor = getDPadColors(active)

    gfx.setColor(fillColor)
    gfx.fillPolygon(table.unpack(points))

    gfx.setColor(borderColor)
    drawClosedPolygon(points)

    gfx.setColor(arrowColor)
    gfx.fillPolygon(table.unpack(arrow))
end

local function applyTheme()
    if theme == THEME_DARK then
        gfx.setColor(gfx.kColorBlack)
        gfx.fillRect(0, 0, SCREEN_W, SCREEN_H)
        gfx.setImageDrawMode(gfx.kDrawModeInverted)
    else
        gfx.setColor(gfx.kColorWhite)
        gfx.fillRect(0, 0, SCREEN_W, SCREEN_H)
        gfx.setImageDrawMode(gfx.kDrawModeCopy)
    end

    gfx.setColor(gfx.kColorBlack)
end

local function drawDPad(activeMode)
    local dir = activeMode or mode

    drawDPadButton(PAD_UP_POINTS, dir == MODE_WORKTIME, PAD_UP_ARROW)
    drawDPadButton(PAD_DOWN_POINTS, dir == MODE_BREAKTIME, PAD_DOWN_ARROW)
    drawDPadButton(PAD_LEFT_POINTS, dir == MODE_SETTINGS, PAD_LEFT_ARROW)
    drawDPadButton(PAD_RIGHT_POINTS, dir == MODE_THEMES, PAD_RIGHT_ARROW)

    gfx.setColor(gfx.kColorBlack)
end

local function updateClockText()
    local t = pd.getTime()

    if cachedClockSecond == t.second then
        return
    end

    cachedClockSecond = t.second
    cachedClockTimeText = string.format("%02d:%02d:%02d", t.hour, t.minute, t.second)
    cachedClockDateText = string.format("%s  %04d-%02d-%02d", WEEKDAYS[t.weekday], t.year, t.month, t.day)
end

local function drawClock()
    updateClockText()

    gfx.setFont(timeFont)
    gfx.drawTextAligned(cachedClockTimeText, SCREEN_CENTER_X, 84, kTextAlignment.center)

    gfx.setFont(defaultFont)
    gfx.drawTextAligned(cachedClockDateText, SCREEN_CENTER_X, 180, kTextAlignment.center)
end

local function drawTimer(timer)
    local remaining = math.max(0, timer.timeLeft)
    local sec = math.floor(remaining / MS_PER_SECOND)
    local min = math.floor(sec / SECONDS_PER_MINUTE)
    sec = sec % SECONDS_PER_MINUTE

    gfx.setFont(timerFont)
    gfx.drawTextAligned(string.format("%02d:%02d", min, sec), SCREEN_CENTER_X, 84, kTextAlignment.center)

    gfx.setFont(defaultFont)

    if loopRunning then
        local phaseLabel = (loopPhase == MODE_WORKTIME) and "WORK" or "BREAK"
        gfx.drawTextAligned(string.format("%s    LOOP  %d / %d", phaseLabel, currentLoop, loopCount), SCREEN_CENTER_X, 190, kTextAlignment.center)
    elseif timer.paused and blinkVisible(BLINK_PERIOD_MS, BLINK_DUTY) then
        gfx.drawTextAligned("start :  press A         reset :  turn Crank", SCREEN_CENTER_X, 190, kTextAlignment.center)
    end
end

local function drawSettingsRow(index, label, value, rowX, rowY, rowW, rowH)
    local normalColor = (theme == THEME_DARK) and gfx.kColorWhite or gfx.kColorBlack

    if settingsIndex == index then
        gfx.setColor(normalColor)
        gfx.fillRoundRect(rowX, rowY - 4, rowW, rowH, 4)
        useSelectedTextMode()
    else
        gfx.setColor(normalColor)
        useNormalTextMode()
    end

    gfx.drawText(label, rowX + 10, rowY)
    gfx.drawTextAligned(value, rowX + rowW - 10, rowY, kTextAlignment.right)
end

local function drawTimerSettings()
    gfx.setFont(defaultFont)

    local rowX = 112
    local rowW = 176
    local rowH = 22
    local firstRowY = 76
    local rowGap = 30
    local normalColor = (theme == THEME_DARK) and gfx.kColorWhite or gfx.kColorBlack

    drawSettingsRow(SETTING_LOOP_INDEX, SETTINGS_LABELS[SETTING_LOOP_INDEX], tostring(loopCount), rowX, firstRowY, rowW, rowH)
    drawSettingsRow(SETTING_WORK_INDEX, SETTINGS_LABELS[SETTING_WORK_INDEX], tostring(workMinutes) .. "m", rowX, firstRowY + rowGap, rowW, rowH)
    drawSettingsRow(SETTING_BREAK_INDEX, SETTINGS_LABELS[SETTING_BREAK_INDEX], tostring(breakMinutes) .. "m", rowX, firstRowY + rowGap * 2, rowW, rowH)

    gfx.setColor(normalColor)
    useNormalTextMode()
    gfx.drawTextAligned("turn Crank to +/- ", SCREEN_CENTER_X, 176, kTextAlignment.center)

    if settingsMessage and pd.getCurrentTimeMilliseconds() < settingsMessageUntil then
        gfx.drawTextAligned(settingsMessage, SCREEN_CENTER_X, 198, kTextAlignment.center)
    else
        gfx.drawTextAligned("start: press A    reset: hold A ", SCREEN_CENTER_X, 198, kTextAlignment.center)
    end

    useNormalTextMode()
    gfx.setColor(gfx.kColorBlack)
end

local function drawThemeSettings()
    gfx.setFont(defaultFont)
    gfx.drawTextAligned("Theme :  " .. theme, SCREEN_CENTER_X, 110, kTextAlignment.center)

    if blinkVisible(BLINK_PERIOD_MS, BLINK_DUTY) then
        gfx.drawTextAligned("press  A  to  switch", SCREEN_CENTER_X, 140, kTextAlignment.center)
    end
end

local function drawModeScreen(screenMode)
    drawDPad(screenMode)

    if screenMode == MODE_CLOCK then
        drawClock()
    elseif screenMode == MODE_WORKTIME then
        drawTimer(workTimer)
    elseif screenMode == MODE_BREAKTIME then
        drawTimer(breakTimer)
    elseif screenMode == MODE_SETTINGS then
        drawTimerSettings()
    elseif screenMode == MODE_THEMES then
        drawThemeSettings()
    end

    gfx.setFont(defaultFont)
    gfx.drawTextAligned(screenMode, SCREEN_CENTER_X, 30, kTextAlignment.center)
end

local function drawModeScreenAt(screenMode, offsetX, offsetY)
    gfx.setDrawOffset(math.floor(offsetX + 0.5), math.floor(offsetY + 0.5))
    drawModeScreen(screenMode)
    gfx.setDrawOffset(0, 0)
end

makeModeImage = function(screenMode)
    local image = gfx.image.new(SCREEN_W, SCREEN_H)

    gfx.pushContext(image)
        applyTheme()
        drawModeScreen(screenMode)
    gfx.popContext()

    return image
end

local function drawPhaseAlert()
    gfx.setFont(timerFont)
    gfx.drawTextAligned(phaseAlertMessage or "DONE", SCREEN_CENTER_X, 84, kTextAlignment.center)

    gfx.setFont(defaultFont)

    if phaseAlertMessage == "FINISHED" then
        gfx.drawTextAligned(string.format("LOOP  %d / %d", loopCount, loopCount), SCREEN_CENTER_X, 158, kTextAlignment.center)
    elseif loopRunning then
        local phaseLabel = (loopPhase == MODE_WORKTIME) and "WORK" or "BREAK"
        gfx.drawTextAligned(string.format("%s    LOOP  %d / %d", phaseLabel, currentLoop, loopCount), SCREEN_CENTER_X, 158, kTextAlignment.center)
    end
end

local function render()
    applyTheme()

    if phaseAlertMessage then
        drawPhaseAlert()
        return
    end

    if transition then
        local elapsed = pd.getCurrentTimeMilliseconds() - transition.start
        local progress = clamp(elapsed / TRANSITION_MS, 0, 1)
        local dx = transition.dx
        local dy = transition.dy

        gfx.setImageDrawMode(gfx.kDrawModeCopy)

        if transitionFromImage then
            transitionFromImage:draw(
                math.floor(dx * progress + 0.5),
                math.floor(dy * progress + 0.5)
            )
        end

        if transitionToImage then
            transitionToImage:draw(
                math.floor(-dx * (1 - progress) + 0.5),
                math.floor(-dy * (1 - progress) + 0.5)
            )
        end

        return
    end

    drawModeScreenAt(mode, 0, 0)
end

-- =====================
-- Init
-- =====================
pd.display.setRefreshRate(REFRESH_RATE)  -- Limit refresh rate for better performance
rebuildTimers()

-- =====================
-- Main loop
-- =====================
playdate.display.setRefreshRate(0)  -- Limit refresh rate for better performance
function pd.update()
    handleInput()
    updateTimer()
    render()
    -- if DEBUG_ENABLED then
    --     pd.drawFPS(5, 5)
    -- end
end
