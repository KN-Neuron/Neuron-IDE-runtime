#include <gtest/gtest.h>
#include <pybind11/embed.h>

#include <cstdint>
#include <exception>
#include <fstream>
#include <memory>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "data_structures/Context.hpp"
#include "data_structures/KeyPress.hpp"
#include "data_structures/Marker.hpp"
#include "events/MarkerEventLink.hpp"
#include "events/Subscription.hpp"
#include "neuronide.pb.h"
#include "parser/Parser.hpp"
#include "scene/Scene.hpp"
#include "scene/SceneObject.hpp"
#include "scene/components/ScriptComponent.hpp"
#include "scene/components/TextRenderer.hpp"
#include "utils/TempDir.hpp"

// The test process runs a Python interpreter for the whole run (TestMain).

namespace {
namespace py = pybind11;

constexpr double        kFrameTime  = 0.25;
constexpr double        kLongFrame  = 0.5;
constexpr double        kObjectSize = 100.0;
constexpr std::uint32_t kFontSize   = 12;
// LSL times of key presses and of a marker, as the scripts below print them.
constexpr double kFirstPress  = 12.5;
constexpr double kSecondPress = 13.25;
constexpr double kStartTime   = 1.5;
// Where MovesObjects puts "Target".
constexpr double kMovedX = 40.0;
constexpr double kMovedY = 102.5;

NeuronIDE::SceneObject* addObject(NeuronIDE::Scene& proto, const std::string& name) {
    auto* object = proto.add_scene_objects();
    object->set_name(name);
    object->set_is_visible(true);
    object->mutable_transform()->set_width(kObjectSize);
    object->mutable_transform()->set_height(kObjectSize);
    return object;
}

// Runs one frame and returns the markers emitted in it.
std::vector<std::string> frame(Scene& scene, double deltaTime = kFrameTime) {
    std::vector<std::string> markers;
    scene.update(Context{deltaTime, &markers});
    return markers;
}

struct FrameOutput {
    std::vector<std::string> markers;
    std::vector<Marker>      timedMarkers;
};

// Runs one frame in which `keys` went down.
FrameOutput frameWithKeys(Scene& scene, std::span<const KeyPress> keys = {}) {
    FrameOutput output;
    scene.update(Context{.timestamp    = kFrameTime,
                         .markers      = &output.markers,
                         .timedMarkers = &output.timedMarkers,
                         .keyPresses   = keys});
    return output;
}

void expectMarker(const Marker& marker, std::string_view name, double time) {
    EXPECT_EQ(marker.eventName, name);
    EXPECT_DOUBLE_EQ(marker.timestamp, time);
}

bool contains(std::string_view text, std::string_view part) {
    return text.find(part) != std::string_view::npos;
}

// The message of the Python exception that evaluating `expression` raises, or empty.
std::string pythonErrorOf(const char* expression) {
    try {
        py::eval(expression);
    } catch (const py::error_already_set& error) {
        return error.what();
    }
    return "";
}

// Scenes of three objects: "Experiment" with the script, "Target" with a TextRenderer, and "Plain"
// with no component.
class ScriptComponentTest : public ::testing::Test {
   protected:
    // A scene whose script is at `scriptPath`, relative to the test's folder.
    std::shared_ptr<Scene> parse(const std::string& scriptPath) {
        NeuronIDE::Scene proto;
        proto.set_project_name("Scripts");
        addObject(proto, "Experiment")
            ->add_components()
            ->mutable_script()
            ->set_script_path(scriptPath);
        auto* text = addObject(proto, "Target")->add_components()->mutable_text();
        text->set_text("before");
        text->set_font_size(kFontSize);
        addObject(proto, "Plain");

        std::stringstream stream;
        proto.SerializeToOstream(&stream);
        return Parser::parseStream(stream, folder.path());
    }

    // A scene running `source`.
    std::shared_ptr<Scene> load(std::string_view source) {
        std::ofstream(folder.path() / "script.py") << source;
        return parse("script.py");
    }

    // The message that loading `source` fails with, or empty.
    std::string loadError(std::string_view source) {
        try {
            load(source);
        } catch (const std::exception& error) {
            return error.what();
        }
        return "";
    }

    static TextRenderer& targetText(const Scene& scene) {
        return *scene.findObject("Target")->findComponent<TextRenderer>();
    }

   private:
    utils::TempDir folder;
};
}  // namespace

TEST_F(ScriptComponentTest, HoldsMarkersFromOnStartForTheFirstFrame) {
    const auto scene = load(R"(
import neuronide

class Trial(neuronide.Script):
    def on_start(self):
        self.emit_marker("started")

    def on_update(self, dt):
        self.emit_marker("updated")
)");

    EXPECT_EQ(frame(*scene), (std::vector<std::string>{"started", "updated"}));
    EXPECT_EQ(frame(*scene), (std::vector<std::string>{"updated"}));
}

TEST_F(ScriptComponentTest, SendsMarkersFromOnStartWithTheFirstFrameEvenIfItsObjectIsInactive) {
    const auto scene = load(R"(
import neuronide

class Trial(neuronide.Script):
    def on_start(self):
        self.emit_marker("started")
        self.obj.active = False
)");

    EXPECT_EQ(frame(*scene), (std::vector<std::string>{"started"}));
    EXPECT_TRUE(frame(*scene).empty());
}

TEST_F(ScriptComponentTest, RunsOnStartWhileTheSceneIsParsed) {
    const auto scene = load(R"(
import neuronide

class Trial(neuronide.Script):
    def on_start(self):
        self.scene.find("Target").text = "set in on_start"
)");

    EXPECT_EQ(targetText(*scene).getText(), "set in on_start");
}

TEST_F(ScriptComponentTest, CallsOnUpdateEveryFrameWithTheFrameTime) {
    const auto scene = load(R"(
import neuronide

class Trial(neuronide.Script):
    def on_start(self):
        self.frames = 0

    def on_update(self, dt):
        self.frames += 1
        self.emit_marker(f"frame {self.frames} took {dt} s")
)");

    EXPECT_EQ(frame(*scene, kFrameTime), (std::vector<std::string>{"frame 1 took 0.25 s"}));
    EXPECT_EQ(frame(*scene, kLongFrame), (std::vector<std::string>{"frame 2 took 0.5 s"}));
}

TEST_F(ScriptComponentTest, ReadsObjects) {
    const auto scene = load(R"(
import neuronide

class Trial(neuronide.Script):
    def on_update(self, dt):
        target = self.scene.find("Target")
        self.emit_marker(f"{self.obj.name} {target.name} {target.active} {target.text}")
)");

    EXPECT_EQ(frame(*scene), (std::vector<std::string>{"Experiment Target True before"}));
}

TEST_F(ScriptComponentTest, ShowsAndHidesObjects) {
    const auto scene  = load(R"(
import neuronide

class Trial(neuronide.Script):
    def on_update(self, dt):
        target = self.scene.find("Target")
        target.active = not target.active
)");
    const auto target = scene->findObject("Target");

    frame(*scene);
    EXPECT_FALSE(target->isActive);
    frame(*scene);
    EXPECT_TRUE(target->isActive);
}

// The script hides "Plain" in the first frame, before it is ever drawn, so that frame fires
// nothing.
TEST_F(ScriptComponentTest, ObjectsItShowsAndHidesFireTheirEventsInTheSameFrame) {
    const auto      scene = load(R"(
import neuronide

class Trial(neuronide.Script):
    def on_update(self, dt):
        plain = self.scene.find("Plain")
        plain.active = not plain.active
)");
    const auto      plain = scene->findObject("Plain");
    MarkerEventLink shown("shown");
    MarkerEventLink hidden("hidden");
    const auto      showing = plain->getEvent("activate")->subscribe(&shown);
    const auto      hiding  = plain->getEvent("deactivate")->subscribe(&hidden);

    EXPECT_TRUE(frame(*scene).empty());
    EXPECT_EQ(frame(*scene), (std::vector<std::string>{"shown"}));
    EXPECT_EQ(frame(*scene), (std::vector<std::string>{"hidden"}));
}

TEST_F(ScriptComponentTest, ChangesText) {
    const auto scene = load(R"(
import neuronide

class Trial(neuronide.Script):
    def on_update(self, dt):
        self.scene.find("Target").text = "after"
)");

    frame(*scene);

    EXPECT_EQ(targetText(*scene).getText(), "after");
}

TEST_F(ScriptComponentTest, PausesWhileItsObjectIsInactive) {
    const auto scene = load(R"(
import neuronide

class Trial(neuronide.Script):
    def on_update(self, dt):
        self.emit_marker("ran")
        self.obj.active = False
)");

    EXPECT_EQ(frame(*scene), (std::vector<std::string>{"ran"}));
    EXPECT_TRUE(frame(*scene).empty());
}

TEST_F(ScriptComponentTest, NeedsNoHooks) {
    const auto scene = load(R"(
import neuronide

class Trial(neuronide.Script):
    pass
)");

    EXPECT_TRUE(frame(*scene).empty());
}

TEST_F(ScriptComponentTest, RejectsAFileWithoutAScriptClass) {
    const std::string error = loadError("import neuronide\n\nTRIALS = 5\n");

    EXPECT_TRUE(contains(error, "defines 0 classes deriving from neuronide.Script")) << error;
}

TEST_F(ScriptComponentTest, RejectsAFileWithTwoScriptClasses) {
    const std::string error = loadError(R"(
import neuronide

class First(neuronide.Script):
    pass

class Second(neuronide.Script):
    pass
)");

    EXPECT_TRUE(contains(error, "defines 2 classes deriving from neuronide.Script")) << error;
}

TEST_F(ScriptComponentTest, ReportsASyntaxError) {
    const std::string error = loadError(R"(
import neuronide

class Trial(neuronide.Script)
    pass
)");

    EXPECT_TRUE(contains(error, "ScriptComponent on 'Experiment'")) << error;
    EXPECT_TRUE(contains(error, "SyntaxError")) << error;
}

TEST_F(ScriptComponentTest, ReportsAnExceptionWhileTheFileRuns) {
    const std::string error = loadError("raise ValueError('bad configuration')\n");

    EXPECT_TRUE(contains(error, "ValueError: bad configuration")) << error;
}

TEST_F(ScriptComponentTest, ReportsAnInitThatSkipsTheBaseClass) {
    const std::string error = loadError(R"(
import neuronide

class Trial(neuronide.Script):
    def __init__(self):
        self.trials = 5
)");

    EXPECT_TRUE(contains(error, "__init__")) << error;
}

TEST_F(ScriptComponentTest, ReportsUseOfTheSceneFromInit) {
    const std::string error = loadError(R"(
import neuronide

class Trial(neuronide.Script):
    def __init__(self):
        super().__init__()
        self.obj.active = False
)");

    EXPECT_TRUE(contains(error, "not attached to a scene object")) << error;
}

TEST_F(ScriptComponentTest, ReportsAnExceptionInOnStartWithItsLine) {
    const std::string error = loadError(R"(import neuronide

class Trial(neuronide.Script):
    def on_start(self):
        raise RuntimeError("no stimuli left")
)");

    EXPECT_TRUE(contains(error, "on_start failed")) << error;
    EXPECT_TRUE(contains(error, "RuntimeError: no stimuli left")) << error;
    EXPECT_TRUE(contains(error, "script.py(5)")) << error;
}

TEST_F(ScriptComponentTest, ReportsAnExceptionInOnUpdate) {
    const auto scene = load(R"(
import neuronide

class Trial(neuronide.Script):
    def on_update(self, dt):
        1 / 0
)");

    try {
        frame(*scene);
        FAIL() << "the exception in on_update was swallowed";
    } catch (const std::runtime_error& error) {
        EXPECT_TRUE(contains(error.what(), "on_update failed")) << error.what();
        EXPECT_TRUE(contains(error.what(), "ZeroDivisionError")) << error.what();
    }
}

TEST_F(ScriptComponentTest, RaisesKeyErrorForAnUnknownObject) {
    const std::string error = loadError(R"(
import neuronide

class Trial(neuronide.Script):
    def on_start(self):
        self.scene.find("Nothing")
)");

    EXPECT_TRUE(contains(error, "KeyError")) << error;
    EXPECT_TRUE(contains(error, "no scene object named 'Nothing'")) << error;
}

TEST_F(ScriptComponentTest, ReportsTextOnAnObjectWithoutATextRenderer) {
    const std::string error = loadError(R"(
import neuronide

class Trial(neuronide.Script):
    def on_start(self):
        self.scene.find("Plain").text = "hello"
)");

    EXPECT_TRUE(contains(error, "'Plain' has no TextRenderer")) << error;
}

TEST_F(ScriptComponentTest, CallsKeyCallbacksWithThePressTimeBeforeOnUpdate) {
    const auto                  scene = load(R"(
import neuronide

class Trial(neuronide.Script):
    def on_start(self):
        self.on_key("space", lambda time: self.emit_marker(f"space at {time}"))
        self.on_key("Return", lambda time: self.emit_marker(f"return at {time}"))

    def on_update(self, dt):
        self.emit_marker("updated")
)");
    const std::vector<KeyPress> presses{{SDL_SCANCODE_RETURN, kFirstPress},
                                        {SDL_SCANCODE_A, kFirstPress},
                                        {SDL_SCANCODE_SPACE, kSecondPress}};

    EXPECT_EQ(frameWithKeys(*scene, presses).markers,
              (std::vector<std::string>{"return at 12.5", "space at 13.25", "updated"}));
    EXPECT_EQ(frameWithKeys(*scene).markers, (std::vector<std::string>{"updated"}));
}

TEST_F(ScriptComponentTest, CallsEveryCallbackBoundToAKeyInTheOrderTheyWereBound) {
    const auto                  scene = load(R"(
import neuronide

class Trial(neuronide.Script):
    def on_start(self):
        self.on_key("Space", lambda time: self.emit_marker("first"))
        self.on_key("Space", self.second)

    def second(self, time):
        self.emit_marker("second")
        self.on_key("Space", lambda time: self.emit_marker("bound by second"))
)");
    const std::vector<KeyPress> press{{SDL_SCANCODE_SPACE, kFirstPress}};

    EXPECT_EQ(frameWithKeys(*scene, press).markers,
              (std::vector<std::string>{"first", "second", "bound by second"}));
}

TEST_F(ScriptComponentTest, IgnoresKeysWhileItsObjectIsInactive) {
    const auto                  scene = load(R"(
import neuronide

class Trial(neuronide.Script):
    def on_start(self):
        self.on_key("Space", lambda time: self.emit_marker("pressed"))
        self.obj.active = False
)");
    const std::vector<KeyPress> press{{SDL_SCANCODE_SPACE, kFirstPress}};

    EXPECT_TRUE(frameWithKeys(*scene, press).markers.empty());
}

TEST_F(ScriptComponentTest, RecordsMarkersWithTheirOwnTime) {
    const auto                  scene = load(R"(
import neuronide

class Trial(neuronide.Script):
    def on_start(self):
        self.emit_marker("started", time=1.5)
        self.on_key("Space", lambda time: self.emit_marker("response", time=time))
)");
    const std::vector<KeyPress> press{{SDL_SCANCODE_SPACE, kFirstPress}};

    const FrameOutput first  = frameWithKeys(*scene, press);
    const FrameOutput second = frameWithKeys(*scene);

    EXPECT_TRUE(first.markers.empty());
    ASSERT_EQ(first.timedMarkers.size(), 2U);
    expectMarker(first.timedMarkers[0], "started", kStartTime);
    expectMarker(first.timedMarkers[1], "response", kFirstPress);
    EXPECT_TRUE(second.timedMarkers.empty());
}

TEST_F(ScriptComponentTest, RejectsAMarkerTimeThatIsNotFinite) {
    const std::string error = loadError(R"(
import neuronide

class Trial(neuronide.Script):
    def on_start(self):
        self.emit_marker("never", time=float("nan"))
)");

    EXPECT_TRUE(contains(error, "ValueError: a marker's time must be finite")) << error;
}

TEST_F(ScriptComponentTest, RejectsAnUnknownKey) {
    const std::string error = loadError(R"(
import neuronide

class Trial(neuronide.Script):
    def on_start(self):
        self.on_key("Spcae", lambda time: None)
)");

    EXPECT_TRUE(contains(error, "ValueError: no key named 'Spcae'")) << error;
}

TEST_F(ScriptComponentTest, RejectsAKeyCallbackThatCannotBeCalled) {
    const std::string error = loadError(R"(
import neuronide

class Trial(neuronide.Script):
    def on_start(self):
        self.on_key("Space", "respond")
)");

    EXPECT_TRUE(contains(error, "TypeError: on_key needs a function to call")) << error;
}

TEST_F(ScriptComponentTest, ReportsAnExceptionInAKeyCallback) {
    const auto                  scene = load(R"(
import neuronide

class Trial(neuronide.Script):
    def on_start(self):
        self.on_key("Space", lambda time: 1 / 0)
)");
    const std::vector<KeyPress> press{{SDL_SCANCODE_SPACE, kFirstPress}};

    try {
        frameWithKeys(*scene, press);
        FAIL() << "the exception in the key callback was swallowed";
    } catch (const std::runtime_error& error) {
        EXPECT_TRUE(contains(error.what(), "a key callback failed")) << error.what();
        EXPECT_TRUE(contains(error.what(), "ZeroDivisionError")) << error.what();
    }
}

TEST_F(ScriptComponentTest, MovesObjects) {
    const auto scene  = load(R"(
import neuronide

class Trial(neuronide.Script):
    def on_start(self):
        target = self.scene.find("Target")
        target.x = 40
        target.y = target.height + 2.5
        self.emit_marker(f"{target.x} {target.y} {target.width}x{target.height}")
)");
    const auto target = scene->findObject("Target");

    EXPECT_EQ(frame(*scene), (std::vector<std::string>{"40.0 102.5 100.0x100.0"}));
    EXPECT_DOUBLE_EQ(target->transform.posX, kMovedX);
    EXPECT_DOUBLE_EQ(target->transform.posY, kMovedY);
}

TEST_F(ScriptComponentTest, RejectsAPositionThatIsNotAFiniteNumberOfPixels) {
    const std::string infinite = loadError(R"(
import neuronide

class Trial(neuronide.Script):
    def on_start(self):
        self.obj.x = float("inf")
)");
    const std::string tooFar   = loadError(R"(
import neuronide

class Trial(neuronide.Script):
    def on_start(self):
        self.obj.y = 1e12
)");

    EXPECT_TRUE(contains(infinite, "ValueError: x must be a finite number of pixels")) << infinite;
    EXPECT_TRUE(contains(tooFar, "ValueError: y must be a finite number of pixels")) << tooFar;
}

TEST_F(ScriptComponentTest, ReportsAMissingFile) {
    try {
        parse("missing.py");
        FAIL() << "a missing script was accepted";
    } catch (const std::runtime_error& error) {
        EXPECT_TRUE(contains(error.what(), "missing.py")) << error.what();
        EXPECT_TRUE(contains(error.what(), "no such file")) << error.what();
    }
}

TEST_F(ScriptComponentTest, RejectsAnEmptyPath) { EXPECT_THROW(parse(""), std::invalid_argument); }

TEST_F(ScriptComponentTest, RejectsANullOwner) {
    EXPECT_THROW(ScriptComponent(nullptr, "script.py"), std::invalid_argument);
}

// A script can keep itself, an object or the scene past the end of the scene; using any of them
// then raises in Python instead of reaching freed memory.
TEST_F(ScriptComponentTest, ScriptsKeptPastTheSceneCanNoLongerReachIt) {
    auto scene = load(R"(
import builtins
import neuronide

class Trial(neuronide.Script):
    def on_start(self):
        builtins.kept_script = self
        builtins.kept_target = self.scene.find("Target")
        builtins.kept_scene = self.scene
)");
    scene.reset();

    const std::string scriptError = pythonErrorOf("kept_script.obj");
    const std::string targetError = pythonErrorOf("kept_target.name");
    const std::string sceneError  = pythonErrorOf("kept_scene.find('Target')");
    py::exec(
        "import builtins\ndel builtins.kept_script\ndel builtins.kept_target\n"
        "del builtins.kept_scene");

    EXPECT_TRUE(contains(scriptError, "not attached to a scene object")) << scriptError;
    EXPECT_TRUE(contains(targetError, "no longer exists")) << targetError;
    EXPECT_TRUE(contains(sceneError, "the scene no longer exists")) << sceneError;
}
