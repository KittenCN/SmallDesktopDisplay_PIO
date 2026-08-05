#include "sdd_sim/simulator_app.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include "sdd_sim/gdiplus_jpeg_decoder.h"
#include "sdd_sim/sdd_sim.h"

namespace sdd_sim {
namespace {

constexpr int kWindowWidth = 1080;
constexpr int kWindowHeight = 760;
constexpr float kScreenX = 20.0F;
constexpr float kScreenY = 20.0F;
constexpr float kScreenSize = 720.0F;
constexpr float kPanelX = 765.0F;
constexpr float kButtonWidth = 130.0F;
constexpr float kButtonHeight = 34.0F;

enum class Action {
  PreviousScenario,
  NextScenario,
  Brightness,
  Rotation,
  Animation,
  ToggleDht,
  TemperatureDown,
  TemperatureUp,
  HumidityDown,
  HumidityUp,
  AqiDown,
  AqiUp,
  Pause,
  Step,
  Screenshot,
  Reset,
};

struct Button {
  SDL_FRect rect;
  const char* label;
  Action action;
};

const std::array<Button, 16> kButtons{{
    {{kPanelX, 75, kButtonWidth, kButtonHeight}, "< Scenario", Action::PreviousScenario},
    {{kPanelX + 145, 75, kButtonWidth, kButtonHeight}, "Scenario >", Action::NextScenario},
    {{kPanelX, 145, kButtonWidth, kButtonHeight}, "Brightness [B]", Action::Brightness},
    {{kPanelX + 145, 145, kButtonWidth, kButtonHeight}, "Rotation [R]", Action::Rotation},
    {{kPanelX, 215, kButtonWidth, kButtonHeight}, "Animation [A]", Action::Animation},
    {{kPanelX + 145, 215, kButtonWidth, kButtonHeight}, "DHT [D]", Action::ToggleDht},
    {{kPanelX, 285, kButtonWidth, kButtonHeight}, "Temp -", Action::TemperatureDown},
    {{kPanelX + 145, 285, kButtonWidth, kButtonHeight}, "Temp +", Action::TemperatureUp},
    {{kPanelX, 355, kButtonWidth, kButtonHeight}, "Humidity -", Action::HumidityDown},
    {{kPanelX + 145, 355, kButtonWidth, kButtonHeight}, "Humidity +", Action::HumidityUp},
    {{kPanelX, 425, kButtonWidth, kButtonHeight}, "AQI -", Action::AqiDown},
    {{kPanelX + 145, 425, kButtonWidth, kButtonHeight}, "AQI +", Action::AqiUp},
    {{kPanelX, 495, kButtonWidth, kButtonHeight}, "Pause [Space]", Action::Pause},
    {{kPanelX + 145, 495, kButtonWidth, kButtonHeight}, "Step [Right]", Action::Step},
    {{kPanelX, 565, kButtonWidth, kButtonHeight}, "Screenshot [S]", Action::Screenshot},
    {{kPanelX + 145, 565, kButtonWidth, kButtonHeight}, "Reset [Home]", Action::Reset},
}};

const char* animationName(AnimationKind kind) noexcept {
  switch (kind) {
    case AnimationKind::None: return "None";
    case AnimationKind::Astronaut: return "Astronaut";
    case AnimationKind::Hutao: return "Hutao";
    case AnimationKind::Miku: return "Miku";
  }
  return "Unknown";
}

void applyScenario(int index, SimulatorState& state) {
  state.reset();
  switch ((index % 4 + 4) % 4) {
    case 0:
      state.weather.city = "上海";
      state.weather.temperatureText = "26";
      state.weather.temperatureCelsius = 26.0F;
      state.weather.humidityText = "65%";
      state.weather.relativeHumidity = 65;
      state.weather.aqi = 42;
      state.weather.weatherCode = 1;
      state.weather.banners = {{"天气 多云", "AQI 优 42", "风向 东南风3级",
                                "今日 多云", "最低温度 22℃", "最高温度 29℃"}};
      break;
    case 1:
      state.weather.city = "北京";
      state.weather.temperatureText = "8";
      state.weather.temperatureCelsius = 8.0F;
      state.weather.humidityText = "38%";
      state.weather.relativeHumidity = 38;
      state.weather.aqi = 118;
      state.weather.weatherCode = 53;
      state.weather.banners = {{"天气 霾", "AQI 轻度 118", "风向 北风4级",
                                "今日 霾", "最低温度 1℃", "最高温度 10℃"}};
      break;
    case 2:
      state.weather.city = "广州";
      state.weather.temperatureText = "31.5";
      state.weather.temperatureCelsius = 31.5F;
      state.weather.humidityText = "96%";
      state.weather.relativeHumidity = 96;
      state.weather.aqi = 76;
      state.weather.weatherCode = 9;
      state.weather.banners = {{"天气 大雨", "AQI 良 76", "风向 南风3级",
                                "今日 大雨", "最低温度 26℃", "最高温度 33℃"}};
      break;
    default:
      state.weather.city = "OFFLINE";
      state.weather.temperatureText = "--";
      state.weather.temperatureCelsius = -10.0F;
      state.weather.humidityText = "--%";
      state.weather.relativeHumidity = 0;
      state.weather.aqi = -1;
      state.weather.weatherCode = 999;
      state.weather.banners = {{"WEATHER WAIT", "AQI --", "", "", "", ""}};
      state.calendar.banners = {{"NTP WAIT", "", "农历未存", "", ""}};
      state.settings.animation = AnimationKind::None;
      break;
  }
}

bool saveScreenshot(const std::vector<std::uint32_t>& pixels,
                    const std::filesystem::path& path) {
  if (pixels.size() != static_cast<std::size_t>(Framebuffer::kWidth * Framebuffer::kHeight)) {
    return false;
  }
  SDL_Surface* surface = SDL_CreateSurfaceFrom(
      Framebuffer::kWidth, Framebuffer::kHeight, SDL_PIXELFORMAT_ARGB8888,
      const_cast<std::uint32_t*>(pixels.data()), Framebuffer::kWidth * 4);
  if (surface == nullptr) {
    return false;
  }
  const std::string utf8Path = path.u8string();
  const bool saved = SDL_SaveBMP(surface, utf8Path.c_str());
  SDL_DestroySurface(surface);
  return saved;
}

void cycleAnimation(SimulatorState& state) {
  switch (state.settings.animation) {
    case AnimationKind::None: state.setAnimation(AnimationKind::Astronaut); break;
    case AnimationKind::Astronaut: state.setAnimation(AnimationKind::Hutao); break;
    case AnimationKind::Hutao: state.setAnimation(AnimationKind::Miku); break;
    case AnimationKind::Miku: state.setAnimation(AnimationKind::None); break;
  }
}

void performAction(Action action, SimulatorState& state, int& scenario,
                   bool& screenshotRequested) {
  switch (action) {
    case Action::PreviousScenario:
      scenario = (scenario + 3) % 4;
      applyScenario(scenario, state);
      break;
    case Action::NextScenario:
      scenario = (scenario + 1) % 4;
      applyScenario(scenario, state);
      break;
    case Action::Brightness: state.cycleBrightness(); break;
    case Action::Rotation: state.setRotation(state.settings.rotation + 1); break;
    case Action::Animation: cycleAnimation(state); break;
    case Action::ToggleDht: state.indoor.enabled = !state.indoor.enabled; break;
    case Action::TemperatureDown:
      state.weather.temperatureCelsius = std::max(-20.0F, state.weather.temperatureCelsius - 1.0F);
      state.weather.temperatureText = std::to_string(static_cast<int>(state.weather.temperatureCelsius));
      break;
    case Action::TemperatureUp:
      state.weather.temperatureCelsius = std::min(50.0F, state.weather.temperatureCelsius + 1.0F);
      state.weather.temperatureText = std::to_string(static_cast<int>(state.weather.temperatureCelsius));
      break;
    case Action::HumidityDown:
      state.weather.relativeHumidity = std::max(0, state.weather.relativeHumidity - 5);
      state.weather.humidityText = std::to_string(state.weather.relativeHumidity) + "%";
      break;
    case Action::HumidityUp:
      state.weather.relativeHumidity = std::min(100, state.weather.relativeHumidity + 5);
      state.weather.humidityText = std::to_string(state.weather.relativeHumidity) + "%";
      break;
    case Action::AqiDown: state.weather.aqi = std::max(-1, state.weather.aqi - 25); break;
    case Action::AqiUp: state.weather.aqi = std::min(500, state.weather.aqi + 25); break;
    case Action::Pause: state.paused = !state.paused; break;
    case Action::Step: state.stepAnimation(); break;
    case Action::Screenshot: screenshotRequested = true; break;
    case Action::Reset:
      applyScenario(scenario, state);
      break;
  }
}

void drawButton(SDL_Renderer* renderer, const Button& button) {
  SDL_SetRenderDrawColor(renderer, 45, 55, 68, 255);
  SDL_RenderFillRect(renderer, &button.rect);
  SDL_SetRenderDrawColor(renderer, 92, 112, 134, 255);
  SDL_RenderRect(renderer, &button.rect);
  SDL_SetRenderDrawColor(renderer, 236, 240, 244, 255);
  SDL_RenderDebugText(renderer, button.rect.x + 9.0F, button.rect.y + 12.0F,
                      button.label);
}

void drawPanel(SDL_Renderer* renderer, const SimulatorState& state, int scenario) {
  char buffer[128]{};
  SDL_SetRenderDrawColor(renderer, 16, 22, 30, 255);
  const SDL_FRect panel{kPanelX - 15.0F, 20.0F, 310.0F, 720.0F};
  SDL_RenderFillRect(renderer, &panel);
  SDL_SetRenderDrawColor(renderer, 125, 211, 252, 255);
  SDL_RenderDebugText(renderer, kPanelX, 34.0F, "SmallDesktopDisplay Simulator");
  SDL_SetRenderDrawColor(renderer, 180, 190, 202, 255);
  std::snprintf(buffer, sizeof(buffer), "Scenario %d/4  Time %02d:%02d:%02d", scenario + 1,
                state.clock.hour, state.clock.minute, state.clock.second);
  SDL_RenderDebugText(renderer, kPanelX, 52.0F, buffer);

  for (const Button& button : kButtons) {
    drawButton(renderer, button);
  }

  SDL_SetRenderDrawColor(renderer, 180, 190, 202, 255);
  std::snprintf(buffer, sizeof(buffer), "Brightness: %d%%   Rotation: %d",
                state.settings.brightness, state.settings.rotation);
  SDL_RenderDebugText(renderer, kPanelX, 185.0F, buffer);
  std::snprintf(buffer, sizeof(buffer), "Animation: %s   DHT: %s",
                animationName(state.settings.animation), state.indoor.enabled ? "ON" : "OFF");
  SDL_RenderDebugText(renderer, kPanelX, 255.0F, buffer);
  std::snprintf(buffer, sizeof(buffer), "Temperature: %.1f C", state.weather.temperatureCelsius);
  SDL_RenderDebugText(renderer, kPanelX, 325.0F, buffer);
  std::snprintf(buffer, sizeof(buffer), "Humidity: %d%%", state.weather.relativeHumidity);
  SDL_RenderDebugText(renderer, kPanelX, 395.0F, buffer);
  std::snprintf(buffer, sizeof(buffer), "AQI: %d", state.weather.aqi);
  SDL_RenderDebugText(renderer, kPanelX, 465.0F, buffer);
  SDL_RenderDebugText(renderer, kPanelX, 535.0F,
                      state.paused ? "Clock/animation paused" : "Clock/animation running");
  SDL_RenderDebugText(renderer, kPanelX, 620.0F, "Keys: 1-4 scenarios, Esc quit");
  SDL_RenderDebugText(renderer, kPanelX, 638.0F, "Screenshot: simulator-screen.bmp");
  SDL_RenderDebugText(renderer, kPanelX, 674.0F,
                      "UI preview only: hardware timing is not simulated.");
}

int selfTest(GdiPlusJpegDecoder& decoder) {
  if (!weatherFont().valid() || !calendarFont().valid()) {
    std::fprintf(stderr, "self-test: VLW font parsing failed\n");
    return 1;
  }
  if (animationFrames(AnimationKind::Astronaut).size() != 10U ||
      animationFrames(AnimationKind::Hutao).size() != 32U ||
      animationFrames(AnimationKind::Miku).size() != 4U) {
    std::fprintf(stderr, "self-test: animation catalog mismatch\n");
    return 1;
  }
  JpegImage image;
  const ByteSpan icon = weatherIconAsset(WeatherIcon::Sunny);
  if (!decoder.decode(icon.data, icon.size, image) || image.width != 60 || image.height != 60) {
    std::fprintf(stderr, "self-test: weather JPEG decode failed\n");
    return 1;
  }
  SimulatorState state;
  Framebuffer framebuffer;
  DisplayRenderer renderer(&decoder);
  renderer.render(state, framebuffer);
  const auto pixels = framebuffer.toArgb8888(0, 100);
  if (pixels.size() != static_cast<std::size_t>(Framebuffer::kWidth * Framebuffer::kHeight)) {
    std::fprintf(stderr, "self-test: framebuffer conversion failed\n");
    return 1;
  }
  std::puts("SmallDesktopDisplay simulator self-test passed");
  return 0;
}

}  // namespace

int runSimulatorApp(int argc, char** argv) {
  bool headless = false;
  bool runSelfTest = false;
  int frames = 1;
  std::filesystem::path screenshotPath;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--headless") == 0) {
      headless = true;
    } else if (std::strcmp(argv[i], "--self-test") == 0) {
      runSelfTest = true;
    } else if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
      frames = std::max(1, std::atoi(argv[++i]));
    } else if (std::strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) {
      screenshotPath = std::filesystem::u8path(argv[++i]);
    } else if (std::strcmp(argv[i], "--help") == 0) {
      std::puts("Usage: SmallDesktopDisplaySimulator [--self-test] [--headless] "
                "[--frames N] [--screenshot output.bmp]");
      return 0;
    }
  }

  GdiPlusSession gdiplus;
  if (!gdiplus.ready()) {
    std::fprintf(stderr, "GDI+ initialization failed\n");
    return 2;
  }
  GdiPlusJpegDecoder jpegDecoder;
  if (runSelfTest) {
    return selfTest(jpegDecoder);
  }

  if (!SDL_Init(headless ? 0 : SDL_INIT_VIDEO)) {
    std::fprintf(stderr, "SDL initialization failed: %s\n", SDL_GetError());
    return 2;
  }

  SimulatorState state;
  Framebuffer framebuffer;
  DisplayRenderer displayRenderer(&jpegDecoder);
  int scenario = 0;
  applyScenario(scenario, state);

  if (headless) {
    for (int frame = 0; frame < frames; ++frame) {
      state.tick(100);
    }
    displayRenderer.render(state, framebuffer);
    auto pixels = framebuffer.toArgb8888(state.settings.rotation, state.settings.brightness);
    if (!screenshotPath.empty() && !saveScreenshot(pixels, screenshotPath)) {
      std::fprintf(stderr, "Could not save screenshot: %s\n", SDL_GetError());
      SDL_Quit();
      return 3;
    }
    SDL_Quit();
    return 0;
  }

  SDL_Window* window = SDL_CreateWindow("SmallDesktopDisplay Simulator", kWindowWidth,
                                        kWindowHeight, 0);
  SDL_Renderer* renderer = window == nullptr ? nullptr : SDL_CreateRenderer(window, nullptr);
  SDL_Texture* texture = renderer == nullptr
                             ? nullptr
                             : SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                                 SDL_TEXTUREACCESS_STREAMING,
                                                 Framebuffer::kWidth, Framebuffer::kHeight);
  if (window == nullptr || renderer == nullptr || texture == nullptr) {
    std::fprintf(stderr, "SDL window creation failed: %s\n", SDL_GetError());
    if (texture != nullptr) SDL_DestroyTexture(texture);
    if (renderer != nullptr) SDL_DestroyRenderer(renderer);
    if (window != nullptr) SDL_DestroyWindow(window);
    SDL_Quit();
    return 2;
  }
  SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);

  bool running = true;
  bool screenshotRequested = false;
  std::uint64_t previousTicks = SDL_GetTicks();
  while (running) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_EVENT_QUIT) {
        running = false;
      } else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
        switch (event.key.key) {
          case SDLK_ESCAPE: running = false; break;
          case SDLK_SPACE: performAction(Action::Pause, state, scenario, screenshotRequested); break;
          case SDLK_RIGHT: performAction(Action::Step, state, scenario, screenshotRequested); break;
          case SDLK_B: performAction(Action::Brightness, state, scenario, screenshotRequested); break;
          case SDLK_R: performAction(Action::Rotation, state, scenario, screenshotRequested); break;
          case SDLK_A: performAction(Action::Animation, state, scenario, screenshotRequested); break;
          case SDLK_D: performAction(Action::ToggleDht, state, scenario, screenshotRequested); break;
          case SDLK_S: performAction(Action::Screenshot, state, scenario, screenshotRequested); break;
          case SDLK_HOME: performAction(Action::Reset, state, scenario, screenshotRequested); break;
          case SDLK_1: scenario = 0; applyScenario(scenario, state); break;
          case SDLK_2: scenario = 1; applyScenario(scenario, state); break;
          case SDLK_3: scenario = 2; applyScenario(scenario, state); break;
          case SDLK_4: scenario = 3; applyScenario(scenario, state); break;
          default: break;
        }
      } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
                 event.button.button == SDL_BUTTON_LEFT) {
        for (const Button& button : kButtons) {
          if (event.button.x >= button.rect.x &&
              event.button.x < button.rect.x + button.rect.w &&
              event.button.y >= button.rect.y &&
              event.button.y < button.rect.y + button.rect.h) {
            performAction(button.action, state, scenario, screenshotRequested);
            break;
          }
        }
      }
    }

    const std::uint64_t nowTicks = SDL_GetTicks();
    const std::uint64_t delta = std::min<std::uint64_t>(nowTicks - previousTicks, 250U);
    previousTicks = nowTicks;
    state.tick(static_cast<std::uint32_t>(delta));
    displayRenderer.render(state, framebuffer);
    auto pixels = framebuffer.toArgb8888(state.settings.rotation, state.settings.brightness);
    SDL_UpdateTexture(texture, nullptr, pixels.data(), Framebuffer::kWidth * 4);

    SDL_SetRenderDrawColor(renderer, 8, 12, 18, 255);
    SDL_RenderClear(renderer);
    const SDL_FRect screenRect{kScreenX, kScreenY, kScreenSize, kScreenSize};
    SDL_RenderTexture(renderer, texture, nullptr, &screenRect);
    SDL_SetRenderDrawColor(renderer, 90, 100, 112, 255);
    SDL_RenderRect(renderer, &screenRect);
    drawPanel(renderer, state, scenario);
    SDL_RenderPresent(renderer);

    if (screenshotRequested) {
      if (!saveScreenshot(pixels, "simulator-screen.bmp")) {
        SDL_Log("Screenshot failed: %s", SDL_GetError());
      }
      screenshotRequested = false;
    }
    SDL_Delay(10);
  }

  SDL_DestroyTexture(texture);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}

}  // namespace sdd_sim
