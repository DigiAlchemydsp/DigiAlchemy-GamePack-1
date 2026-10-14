# DigiAlchemy GamePack — releases

Prebuilt `.elemod` files. An `.elemod` is a **linkable elekloader mod**, not a
firmware image: to flash it you combine it with the **core** and **your own**
stock Digitone OS on your machine. No firmware is distributed here.

## Latest — `digidoom-0.21.elemod` (v0.21)

A pocket arcade for the Digitone screen: press **`MIDI+YES`** for a game-select
menu, then play **DOOM** (a from-scratch DOOM-style raycaster), **INVADERS**
(lightweight Space Invaders) or **SNAKE** (grows and speeds up per dot).

### Requirements
- Elektron **Digitone mk1 / Digitone Keys, OS 1.43**
- [elekloader](https://github.com/irpina/elekloader) and the **core** mod
  (`core-dn1-2.0a.elemod`, from elekloader's `mods/core-dn1`)
- your own stock `Digitone_and_Digitone_Keys_OS1.43.syx`

### Build the firmware
```powershell
python -m elekloader.patch --stock Digitone_and_Digitone_Keys_OS1.43.syx `
    --mod core-dn1-2.0a.elemod --mod digidoom-0.21.elemod `
    --out custom.syx --version 0.21
```
…or add `core-dn1-2.0a.elemod` and `digidoom-0.21.elemod` in elekloader's
window and build the `.syx`.

### Controls
| | |
|---|---|
| **MIDI + YES** | open / close the game-select menu |
| **menu** | `UP`/`DOWN` select · `YES` start · `NO` exit |
| **DOOM** | arrows move / turn · `YES` fire · `NO` use a door |
| **INVADERS** | `LEFT`/`RIGHT` move · `YES` fire |
| **SNAKE** | arrows steer |

### Recovery
Hold **FUNC** while powering on for the startup menu, press **TRIG 4**
(OS UPGRADE), then send the stock `.syx`.

## Licensing

Our mods are **GPL-2.0-or-later**. No firmware is distributed here — you build
the `.syx` from your own stock OS. See [`../NOTICE`](../NOTICE) and
[`../LICENSE`](../LICENSE).
