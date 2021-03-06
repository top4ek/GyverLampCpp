#include "EffectsManager.h"
#include "Settings.h"
#include "MqttClient.h"
#include "LampWebServer.h"

#include "effects/basic/SparklesEffect.h"
#include "effects/basic/FireEffect.h"
#include "effects/basic/MatrixEffect.h"
#include "effects/basic/VerticalRainbowEffect.h"
#include "effects/basic/HorizontalRainbowEffect.h"
#include "effects/basic/ColorEffect.h"
#include "effects/basic/ColorsEffect.h"
#include "effects/basic/SnowEffect.h"
#include "effects/basic/LightersEffect.h"
#include "effects/basic/ClockEffect.h"
#include "effects/basic/ClockHorizontal1Effect.h"
#include "effects/basic/ClockHorizontal2Effect.h"
#include "effects/basic/ClockHorizontal3Effect.h"
#include "effects/basic/StarfallEffect.h"
#include "effects/basic/DiagonalRainbowEffect.h"

#include "effects/noise/MadnessNoiseEffect.h"
#include "effects/noise/CloudNoiseEffect.h"
#include "effects/noise/LavaNoiseEffect.h"
#include "effects/noise/PlasmaNoiseEffect.h"
#include "effects/noise/RainbowNoiseEffect.h"
#include "effects/noise/RainbowStripeNoiseEffect.h"
#include "effects/noise/ZebraNoiseEffect.h"
#include "effects/noise/ForestNoiseEffect.h"
#include "effects/noise/OceanNoiseEffect.h"

#include "effects/sound/SoundEffect.h"
#include "effects/sound/SoundStereoEffect.h"

#include "effects/basic/WaterfallEffect.h"
#include "effects/basic/TwirlRainbowEffect.h"
#include "effects/basic/PulseCirclesEffect.h"
#include "effects/basic/AnimationEffect.h"
#include "effects/basic/StormEffect.h"
#include "effects/basic/Matrix2Effect.h"
#include "effects/basic/TrackingLightersEffect.h"
#include "effects/basic/LightBallsEffect.h"
#include "effects/basic/MovingCubeEffect.h"
#include "effects/basic/WhiteColorEffect.h"

#include "effects/fractional/PulsingCometEffect.h"
#include "effects/fractional/DoubleCometsEffect.h"
#include "effects/fractional/TripleCometsEffect.h"
#include "effects/fractional/RainbowCometEffect.h"
#include "effects/fractional/ColorCometEffect.h"
#include "effects/fractional/MovingFlameEffect.h"
#include "effects/fractional/FractorialFireEffect.h"
#include "effects/fractional/RainbowKiteEffect.h"

#include "effects/basic/BouncingBallsEffect.h"
#include "effects/basic/SpiralEffect.h"
#include "effects/basic/MetaBallsEffect.h"
#include "effects/basic/SinusoidEffect.h"
#include "effects/basic/WaterfallPaletteEffect.h"
#include "effects/basic/RainEffect.h"
#include "effects/basic/PrismataEffect.h"

#include "effects/aurora/FlockEffect.h"
#include "effects/aurora/WhirlEffect.h"
#include "effects/aurora/WaveEffect.h"

#include "effects/basic/Fire12Effect.h"
#include "effects/basic/Fire18Effect.h"
#include "effects/basic/RainNeoEffect.h"
#include "effects/basic/TwinklesEffect.h"

#include "effects/network/DMXEffect.h"

#include <map>

namespace  {

EffectsManager *object = nullptr;

std::map<String, Effect*> effectsMap;

uint32_t effectTimer = 0;

uint8_t activeIndex = 0;

} // namespace

EffectsManager *EffectsManager::instance()
{
    return object;
}

void EffectsManager::Initialize()
{
    if (object) {
        return;
    }

    Serial.println(F("Initializing EffectsManager"));
    object = new EffectsManager();
}

void EffectsManager::processEffectSettings(const JsonObject &json)
{
    const String effectId = json[F("i")].as<String>();

    if (effectsMap.count(effectId) <= 0) {
        Serial.print(F("Missing effect: "));
        Serial.println(effectId);
        return;
    }

    Effect *effect = effectsMap[effectId];
    effects.push_back(effect);
    effect->initialize(json);
}

void EffectsManager::processAllEffects()
{
    for (auto effectPair : effectsMap) {
        effects.push_back(effectPair.second);
    }
}

void EffectsManager::loop()
{
    if (activeIndex >= effects.size()) {
        return;
    }

    if (effectTimer != 0 && (millis() - effectTimer) < activeEffect()->settings.speed) {
        return;
    }
    effectTimer = millis();

    activeEffect()->Process();
}

void EffectsManager::next()
{
    activeEffect()->deactivate();
    myMatrix->clear();
    if (activeIndex == effects.size() - 1) {
        activeIndex = 0;
    } else {
        ++activeIndex;
    }
    activateEffect(activeIndex);
}

void EffectsManager::previous()
{
    activeEffect()->deactivate();
    myMatrix->clear();
    if (activeIndex == 0) {
        activeIndex = static_cast<uint8_t>(effects.size() - 1);
    } else {
        --activeIndex;
    }
    activateEffect(activeIndex);
}

void EffectsManager::changeEffectByName(const String &name)
{
    for (size_t index = 0; index < effects.size(); index++) {
        Effect *effect = effects[index];
        if (effect->settings.name == name) {
            activateEffect(index);
            break;
        }
    }
}

void EffectsManager::changeEffectById(const String &id)
{
    for (size_t index = 0; index < effects.size(); index++) {
        Effect *effect = effects[index];
        if (effect->settings.id == id) {
            activateEffect(index);
            break;
        }
    }
}

void EffectsManager::activateEffect(uint8_t index)
{
    if (index >= effects.size()) {
        index = 0;
    }
    myMatrix->clear();
    activeEffect()->deactivate();
    if (activeIndex != index) {
        activeIndex = index;
    }
    Effect *effect = effects[index];
    myMatrix->setBrightness(effect->settings.brightness);
    Serial.printf_P(PSTR("Activating effect[%u]: %s\n"), index, effect->settings.name.c_str());
    effect->activate();
    mqtt->update();
    lampWebServer->update();
    mySettings->saveLater();
}

void EffectsManager::updateCurrentSettings(const JsonObject &json)
{
    activeEffect()->initialize(json);
    myMatrix->setBrightness(activeEffect()->settings.brightness);
    mySettings->saveLater();
}

void EffectsManager::updateSettingsById(const String &id, const JsonObject &json)
{
    for (size_t index = 0; index < effects.size(); index++) {
        Effect *effect = effects[index];
        if (effect->settings.id == id) {
            if (effect != effects[activeIndex]) {
                activateEffect(activeIndex);
            }
            effect->initialize(json);
            break;
        }
    }
    myMatrix->setBrightness(activeEffect()->settings.brightness);
    mySettings->saveLater();
}

uint8_t EffectsManager::count()
{
    return static_cast<uint8_t>(effects.size());
}

Effect *EffectsManager::activeEffect()
{
    if (effects.size() > activeIndex) {
        return effects[activeIndex];
    }

    return nullptr;
}

uint8_t EffectsManager::activeEffectIndex()
{
    return activeIndex;
}

template <typename T>
void RegisterEffect(const String &id)
{
    effectsMap[id] = new T(id);
}

EffectsManager::EffectsManager()
{
    randomSeed(micros());

    effectsMap[PSTR("Sparkles")] = new SparklesEffect();
    effectsMap[PSTR("Fire")] = new FireEffect();
    effectsMap[PSTR("VerticalRainbow")] = new VerticalRainbowEffect();
    effectsMap[PSTR("HorizontalRainbow")] = new HorizontalRainbowEffect();
    effectsMap[PSTR("Colors")] = new ColorsEffect();
    effectsMap[PSTR("MadnessNoise")] = new MadnessNoiseEffect();
    effectsMap[PSTR("CloudNoise")] = new CloudNoiseEffect();
    effectsMap[PSTR("LavaNoise")] = new LavaNoiseEffect();
    effectsMap[PSTR("PlasmaNoise")] = new PlasmaNoiseEffect();
    effectsMap[PSTR("RainbowNoise")] = new RainbowNoiseEffect();
    effectsMap[PSTR("RainbowStripeNoise")] = new RainbowStripeNoiseEffect();
    effectsMap[PSTR("ZebraNoise")] = new ZebraNoiseEffect();
    effectsMap[PSTR("ForestNoise")] = new ForestNoiseEffect();
    effectsMap[PSTR("OceanNoise")] = new OceanNoiseEffect();
    effectsMap[PSTR("Color")] = new ColorEffect();
    effectsMap[PSTR("Snow")] = new SnowEffect();
    effectsMap[PSTR("Matrix")] = new MatrixEffect();
    effectsMap[PSTR("Lighters")] = new LightersEffect();
    effectsMap[PSTR("Clock")] = new ClockEffect();
    effectsMap[PSTR("ClockHorizontal1")] = new ClockHorizontal1Effect();
    effectsMap[PSTR("ClockHorizontal2")] = new ClockHorizontal2Effect();
    effectsMap[PSTR("ClockHorizontal3")] = new ClockHorizontal3Effect();
    effectsMap[PSTR("Starfall")] = new StarfallEffect();
    effectsMap[PSTR("DiagonalRainbow")] = new DiagonalRainbowEffect();
    effectsMap[PSTR("Waterfall")] = new WaterfallEffect();
    effectsMap[PSTR("TwirlRainbow")] = new TwirlRainbowEffect();
    effectsMap[PSTR("PulseCircles")] = new PulseCirclesEffect();
    effectsMap[PSTR("Animation")] = new AnimationEffect();
    effectsMap[PSTR("Storm")] = new StormEffect();
    effectsMap[PSTR("Matrix2")] = new Matrix2Effect();
    effectsMap[PSTR("TrackingLighters")] = new TrackingLightersEffect();
    effectsMap[PSTR("LightBalls")] = new LightBallsEffect();
    effectsMap[PSTR("MovingCube")] = new MovingCubeEffect();
    effectsMap[PSTR("WhiteColor")] = new WhiteColorEffect();
    effectsMap[PSTR("PulsingComet")] = new PulsingCometEffect();
    effectsMap[PSTR("DoubleComets")] = new DoubleCometsEffect();
    effectsMap[PSTR("TripleComets")] = new TripleCometsEffect();
    effectsMap[PSTR("RainbowComet")] = new RainbowCometEffect();
    effectsMap[PSTR("ColorComet")] = new ColorCometEffect();
    effectsMap[PSTR("MovingFlame")] = new MovingFlameEffect();
    effectsMap[PSTR("FractorialFire")] = new FractorialFireEffect();
    effectsMap[PSTR("RainbowKite")] = new RainbowKiteEffect();
    effectsMap[PSTR("BouncingBalls")] = new BouncingBallsEffect();
    effectsMap[PSTR("Spiral")] = new SpiralEffect();
    effectsMap[PSTR("MetaBalls")] = new MetaBallsEffect();
    effectsMap[PSTR("Sinusoid")] = new SinusoidEffect();
    effectsMap[PSTR("WaterfallPalette")] = new WaterfallPaletteEffect();
    effectsMap[PSTR("Rain")] = new RainEffect();
    effectsMap[PSTR("Prismata")] = new PrismataEffect();
    effectsMap[PSTR("Flock")] = new FlockEffect();
    effectsMap[PSTR("Whirl")] = new WhirlEffect();
    effectsMap[PSTR("Wave")] = new WaveEffect();
    effectsMap[PSTR("Fire12")] = new Fire12Effect();
    effectsMap[PSTR("Fire18")] = new Fire18Effect();
    effectsMap[PSTR("RainNeo")] = new RainNeoEffect();
    effectsMap[PSTR("Twinkles")] = new TwinklesEffect();

    // Uncomment to use
    // effectsMap[PSTR("Sound")] = new SoundEffect();
    // effectsMap[PSTR("Stereo")] = new SoundStereoEffect();
    effectsMap[PSTR("DMX")] = new DMXEffect();
}
