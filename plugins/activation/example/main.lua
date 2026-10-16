-- Minimal activation flow.  Needs activation.dll next to crayon and a key from crayon_keygen:
--   crayon_keygen --product demo --features 0 --days 365
function crayon.config(t)
    t.plugins.activation = true
end

local keyText, message = "", "Enter your license key"

function crayon.init()
    crayon.activation.configure{ product = "demo", features = { "bonusLevel" }, trialDays = 3 }
end

-- fires when the status changes while the game runs (license/trial expires)
function crayon.onActivationChanged(new, old) 
    message = "License status: " .. new 
end

function crayon.textinput(s) 
    keyText = keyText .. s 
end

function crayon.keydown(k)
    if k == "backspace" then keyText = keyText:sub(1, -2)
    elseif k == "return" then
        local ok, info, msg = crayon.activation.activate(keyText)
        message = ok and "Activated!" or msg
    end
end

function crayon.draw()
    local A = crayon.activation
    crayon.graphics.clear(0.08, 0.08, 0.12)
    crayon.graphics.drawText("Status: " .. A.getStatus(), 10, 10)
    crayon.graphics.drawText("Machine ID: " .. A.getMachineId(), 10, 24)
    if A.isTrial() then crayon.graphics.drawText("Trial days left: " .. A.getTrialDaysLeft(), 10, 38) end
    if A.hasFeature("bonusLevel") then crayon.graphics.drawText("Bonus level unlocked", 10, 52) end
    if not A.isAllowed() then
        crayon.graphics.drawText(message, 10, 80)
        crayon.graphics.drawText("> " .. keyText, 10, 94)
    end
end
