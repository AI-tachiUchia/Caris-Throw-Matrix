# Caris Throw Matrix — Native AU/VST3 Plugin (v0.2.0)

Ein eigenständiges Audio-Effekt-Plugin (AU für Logic Pro & VST3) für **Reverb- & Delay-Throws auf Knopfdruck**, inklusive **Sidechain-Input**, **internem Throw-Recorder mit Drag-&-Drop-WAV direkt in die DAW**, **Mono→Stereo-Support** und **7 Throw-Algorithmen** (inkl. Adlib Spring Reverb, Distorted Pitch & Phaser Swirl).

---

## Neu in Version 0.2.0

1. **Echter Mono → Stereo Support in Logic Pro**
   - Auf einer **Mono-Vocal-Spur** in Logic Pro lässt sich das Plugin jetzt direkt als **`Audio Units → Caris → Caris Throw Matrix → Mono -> Stereo`** laden.
   - Deine Vocal bleibt vorne zentriert, aber der Delay-/Reverb-Throw öffnet sich in vollem Stereo!
   - Unterstützt weiterhin `Stereo -> Stereo` sowie `Mono -> Mono`.

2. **Sidechain-Input & Wet-Only (Kill Dry) Modus**
   - Im Logic-Plugin-Fenster oben rechts unter **`Side Chain`** kannst du jetzt jede beliebige Spur (z. B. deine Lead-Vocal) als Quelle auswählen.
   - Das Plugin erkennt ein anliegendes Sidechain-Signal automatisch (oder per Klick auf **`SIDECHAIN IN`**) und schaltet automatisch auf **100 % Wet**, damit die trockene Vocal nicht doppelt wiedergegeben wird.

3. **Interner Throw-Recorder + Drag & Drop WAV direkt in Logic Pro**
   - **Auto-Capture:** Sobald du **`HOLD TO THROW`** (Mausklick oder Leertaste) drückst, schneidet der interne Recorder den Stereo-Throw **inklusive komplettem Hall-/Delay-Tail** automatisch mit.
   - **REC PASS (Ganzer Durchgang):** Klicke auf **`REC PASS`**, lass deinen Song in Logic laufen und feuere mehrere Throws im Timing ab. Klicke auf **`STOP REC`** – alle Throws samt Pausen dazwischen sind als ein durchgehender Stereo-Take gespeichert.
   - **Drag & Drop to DAW:** Ziehe die Wellenform aus dem Feld **`DRAG WAV TO LOGIC TRACK`** einfach mit der Maus direkt auf eine Audiospur in Logic Pro!
   - Gespeicherte Takes liegen zusätzlich unter `~/Music/CarisThrowMatrix/Captures/` (Button **`FOLDER`** öffnet den Ordner im Finder).

4. **7 Effekt-Modi + eigene Pitch-, Phaser- & Distortion-Kontrolle**
   - **`Dub Delay`**: Klassisches gefiltertes Dub-Echo
   - **`Space Reverb`**: Großer, dichter Hall-Throw
   - **`Shimmer Verb`**: Oktav-Shimmer-Reverb
   - **`Tape PingPong`**: Stereo-Ping-Pong-Delay mit analogem Tape-Wow
   - **`Spring Reverb (Adlib)`**: Metallisch-federnder Adlib-Spring-Reverb mit 8-stufiger Dispersion-Chirp-Kaskade („Drip/Boing“-Transient) und gekoppelter Feder-Tank-Modulation
   - **`Distorted Pitch Throw`**: Gepitchter (-12 st Sub + +12 st Octave oder frei per `PITCH (ST)` wählbar) und asymmetrisch angezerrter Throw mit Hall-Tail
   - **`Phaser Swirl Throw`**: 6-stufiger Stereo-Allpass-Phaser mit Quadratur-LFO im Ping-Pong-/Reverb-Feedback-Loop
   - **Neue Regler in allen Modi:**
     - **`PITCH (ST)`** (`-12` bis `+12` Halbtöne): Granularer Echtzeit-Pitch-Shifter auf dem Throw-Signal
     - **`PHASER`** (`0` bis `100 %`): Stereo-Phaser-Swirl für jeden beliebigen Modus
     - **`DRIVE / DIST`** (`0` bis `100 %`): Mehrstufige harmonische Sättigung ohne DC-Offset

---

## Build & Installation auf macOS (Logic Pro)

Voraussetzungen: macOS, **Xcode**, **CMake** (`brew install cmake`).

```bash
cd CarisThrowMatrixPrototype
./build-macos.sh
./install-macos.sh
```

Danach Logic Pro neu starten und das Plugin unter **Audio FX → Audio Units → Caris → Caris Throw Matrix** (`Mono -> Stereo` oder `Stereo`) laden.

---

## Headless DSP-Tests & Demo-WAVs

```bash
cmake -S CarisThrowMatrixPrototype -B CarisThrowMatrixPrototype/build-dsp -DCARIS_DSP_ONLY=ON
cmake --build CarisThrowMatrixPrototype/build-dsp
ctest --test-dir CarisThrowMatrixPrototype/build-dsp --output-on-failure
./CarisThrowMatrixPrototype/build-dsp/caris_throw_demo CarisThrowMatrixPrototype/Demos
```
