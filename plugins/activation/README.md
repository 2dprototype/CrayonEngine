# activation plugin

Offline game activation for Crayon games: signed license keys, optional machine binding, expiry dates,
a built-in trial, feature flags and clock-rollback / file-tamper detection. No server needed.

## Use it

```lua
function crayon.config(t) t.plugins.activation = true end

function crayon.init()
    crayon.activation.configure{ product = "mygame", features = {"dlc1", "editor"}, trialDays = 7 }
    if not crayon.activation.isAllowed() then  -- not activated and trial over -> show your key screen
    end
end

local ok, info_or_code, msg = crayon.activation.activate("ABCDEF-...")
```

| function | returns |
|---|---|
| `configure{product, features={...}, trialDays=0}` | `true` (call once in `crayon.init`; starts the trial on first run) |
| `activate(key)` | `true, info` or `false, code, message` — codes: `malformed` `bad_signature` `wrong_product` `wrong_machine` `expired` `tampered` `io` |
| `deactivate()` | `true` |
| `getStatus()` | `"activated"` `"trial"` `"expired"` `"trial_expired"` `"unlicensed"` `"tampered"` |
| `isActivated()` / `isTrial()` / `isAllowed()` | bool (`isAllowed` = activated or in trial) |
| `getTrialDaysLeft()` | integer |
| `hasFeature(name)` | bool (activated licenses only) |
| `getInfo()` | `{status, edition, serial, machineBound, activatedAt, expires?, daysLeft?, features={name=true}}` or `nil` |
| `getEdition()` | integer |
| `getMachineId()` | `"A1B2-C3D4-E5F6-0718"` — customers send this for machine-bound keys |
| `getLicensePath()` | path of the stored `license.dat` |

`crayon.onActivationChanged(new, old)` is called (checked every 30 s) when a license or trial expires while the game runs.

## Build + generate keys

```
cmake -S plugins -B build/plugins -G Ninja -DCRAYON_ACTIVATION_SECRET="<long random string>"
plugins\build.bat activation          (also builds tools\crayon_keygen)
```

The secret is compiled into both `activation.dll` and `crayon_keygen`; keys only verify with the same secret.
Keep `crayon_keygen` and the secret **private** - ship only `activation.dll`.

```
crayon_keygen --product mygame --count 10                         # 10 permanent keys, any machine
crayon_keygen --product mygame --features 0,1 --edition 2         # unlocks "dlc1" and "editor" (indices into features={})
crayon_keygen --product mygame --expires 2027-12-31               # or --days 365
crayon_keygen --product mygame --machine A1B2-C3D4-E5F6-0718      # bound to one computer
```

## Security notes

- Keys are 240-bit, HMAC-SHA256 signed (truncated 80-bit tag), checked in constant time.
- `license.dat` (in `%APPDATA%\CrayonActivation\<product>\`, `~/.local/share/...`, `~/Library/Application Support/...`) is bound to the machine and MAC-protected; editing/copying it => `tampered`.
- The scheme is symmetric: whoever extracts the secret from the DLL can forge keys. That stops casual sharing and
  editing, not a determined cracker. Add your own obfuscation / packing if that matters.
- No online checks, so a key can't be revoked and one non-machine-bound key works on any number of PCs
  (use `--machine` keys for per-PC licenses).
