# Modular Function Format

**Python, C++, and ROS 2**

Every part of a software implementation should be understandable from first principles. Code is therefore assembled from small building blocks whose bodies read like the mathematics or pseudocode they implement. Follow [example_format.py](example_format.py) for Python, with the spacing specified below, and the C++ format given in section 4.

---

## At a glance

- **Two kinds of function.** Mathematical building blocks implement one calculation directly. Higher-level functions are made of calls to building blocks. A rare hybrid has a simple formula plus one or two calls at the top.
- **Directness first.** Match the pseudocode. Prefer the explicit inverse to a solver. Note the robust alternative in a comment.
- **Prefixes state intent.** `compute_`, `get_`, `set_`, `plot_`, `render_`, `load_`, `save_`, `run_`, `update_`, `apply_`, `build_`, `solve_`, `visualize_`.
- **Documentation first, then two blank lines, then pseudocode, then two blank lines, then code.** The same layout in Python and C++.
- **No assertions or checks, no extra helpers, and never rename the owner's variables.**
- **ROS is a thin layer.** The core never imports ROS. Nodes convert, call, and publish.

## Contents

1. [Function types](#1-function-types)
2. [Naming and prefixes](#2-naming-and-prefixes)
3. [Python format](#3-python-format)
4. [C++ format](#4-c-format)
5. [Keeping IntelliSense working](#5-keeping-intellisense-working)
6. [ROS 2](#6-ros-2)
7. [Rules for AI agents working in this code](#7-rules-for-ai-agents-working-in-this-code)

---

## 1. Function types

There are two types of function, plus a rare hybrid.

| Type | Contains | Calls other functions? |
|---|---|---|
| **Mathematical building block** | Exactly one calculation, matching the pseudocode | No (only the numerical library: `numpy`, `Eigen`) |
| **Higher-level function** | A sequence of calls, each intermediate result named | Yes, almost entirely |
| **Hybrid** (rare) | A simple formula | One or two calls at the top |

### 1.1 Mathematical building blocks (lowest level)

These implement one mathematical calculation directly, so that the code matches the pseudocode line for line.

- Contain exactly *one* calculation. A multi-step calculation is not written inside a building block. It is either split into several building blocks that a higher-level function composes, or its intermediate results are passed in as parameters. Do not, however, separate a calculation arbitrarily: a single formula stays together.
- Prioritize directness and straightforwardness over robustness and efficiency, until told otherwise. The implementation should be the most intuitive transcription of the mathematics. For example, write the explicit matrix inverse, `np.linalg.inv( A ) @ b`, rather than `np.linalg.solve( A, b )`, because the inverse is the direct statement of the math.
- A more robust or efficient alternative may be recorded in a comment inside the pseudocode block (`# robust alternative: np.linalg.solve( A, b )`). The implementation itself is changed to the robust form only when asked, normally after the early design stage.
- Do not call other functions, other than the numerical library itself (`numpy`, `Eigen`).

### 1.2 Higher-level functions

These are built from calls to lower-level functions (building blocks or other higher-level functions). They may rarely contain a simple direct expression, but for the most part their bodies are a sequence of calls, with each intermediate result named.

```python
def foo( a : np.ndarray ) -> np.ndarray:
    b = f1( a )
    c = f2( b )
    return c
```

### 1.3 Hybrid functions (rare)

A hybrid contains a simple mathematical implementation together with one or two calls to other functions at the top. Use this only where the mathematics is simple. The most common case is a matrix product for a change of coordinate frame: obtain the homogeneous point with a call, then multiply.

---

## 2. Naming and prefixes

Use lowercase `snake_case`. Prefer full words to abbreviations, especially for variables. Conventional abbreviations and established mathematical notation (`T_sb`, `P`, `K`, `R`) are the only exceptions. Preserve existing variable names, notation, and naming style when editing code; never shorten, simplify, or rename them.

### 2.1 Building block prefixes

| Prefix | Meaning |
|---|---|
| `compute_` | A mathematical calculation that returns a value. This is the single prefix for calculations. |
| `get_` | Extraction of a value without calculation (a slice, a field, a stacked array). |
| `set_` | Assign a value to a property of an existing object. It changes that object and builds nothing new. |
| `plot_` | One reusable plot, drawn onto supplied axes. |
| `render_` | One reusable non-plot drawing: a scene, a grid or map image, a robot pose. |
| `load_`, `save_` | File input and output, such as reading a bag file. They are separate from `get_` and `set_` because they have side effects. |

### 2.2 Higher-level prefixes

| Prefix | Meaning |
|---|---|
| `run_` | Execute a complete multi-step algorithm or pipeline end to end, including its loop over time or data. It is the entry point. |
| `update_` | One recursive step that folds new information into a current state (a filter measurement update, one optimizer iteration, a map update). It returns the new state. |
| `apply_` | Apply a defined operation to supplied data (a motion model, a gate, a filter, or a frame transform). |
| `build_` | Construct and return a new structure from inputs by composing calls (a matrix, a cost map, a graph). |
| `solve_` | Run a solver or search to completion (least squares, a QP, A*). |
| `visualize_` | Present results to a person by composing `plot_` and `render_` blocks. It also covers animations, saved figures, and published markers, not only plots. |

### 2.3 Notes

- `set_` and `build_` do not overlap. `set_` mutates an object that already exists and performs no calculation. `build_` returns a new object assembled from other function calls.
- A Kalman filter is `run_kalman_filter`, composed of `apply_` steps (the motion model applied to the state) and `update_` steps (the measurement update). A single prediction formula on its own is a `compute_` building block.
- `implement_` is not used. Every function implements something, so the word carries no information. Choose the prefix that states what the function does.
- If a function does not fit a prefix, ask before inventing a new one.

---

## 3. Python format

**Structure**

- Write small, modular functions that express one calculation or extraction from first principles. Do not create additional helpers unless requested.
- Place all imports at the top of the file, never inside functions. Do not use wildcard imports.

**Documentation**

- Annotate every parameter and the return type: `parameter : type` and `-> type`.
- Use a NumPy-style docstring: a concise summary, `Parameters`, and `Returns`. State array and matrix shapes, physical units, and necessary conventions. Add other sections only when useful.
- After the docstring, leave **two empty lines**, then concise mathematical pseudocode in `#` comments. Describe the output only when it is not obvious.

**Layout**

- Leave **two empty lines** between pseudocode and implementation, between logical implementation groups, and before the final `return`.
- Use four-space indentation. Match the example's call spacing, such as `np.asarray( T_sb, dtype = float )`, and space operators consistently.
- Use vertical alignment for code formatting as much as possible ([wikipedia](https://en.wikipedia.org/wiki/Programming_style#Vertical_alignment)).

**Checks and comments**

- Do not add assertions unless explicitly requested. Do not add checks unless explicitly specified. A check is anything that raises a `ValueError` or a similar exception, such as for an incorrectly formatted parameter.
- Preserve existing code comments. No information may be lost and no comment overwritten. If needed, comments may be moved to the bottom; only the owner deletes them.

### 3.1 Blank lines around prints, comments, and docstrings

| Location | Empty lines |
|---|---|
| After the docstring | 2 |
| After the pseudocode | 2 |
| Between logical implementation groups | 2 |
| Before the final `return` | 2 |
| Around a `print` statement | At least 1 above and 1 below |
| Around a comment or docstring beside code | At least 1 |

The two-empty-line separations take precedence over the one-empty-line minimums.

Python `print` calls use f-strings as much as possible, never `%` formatting, `str.format`, or concatenation. Name the quantity, include units where physical, and apply a format specifier to numbers, for example `print( f"Position error: {position_error:.3f} m" )`. The same f-string rule applies to displayed text in plots (section 3.5).

### 3.2 Building block example

```python
import numpy as np


def compute_body_error_transform( T_sb : np.ndarray, T_sd : np.ndarray ) -> np.ndarray:
    """
    Compute the body-frame error transform.

    Parameters
    ----------
    T_sb : array_like, shape (4, 4)
        Current end-effector pose.
    T_sd : array_like, shape (4, 4)
        Desired end-effector pose.

    Returns
    -------
    ndarray, shape (4, 4)
        Body-frame error transform.
    """


    # pseudo-code:
    # T_bd = inverse(T_sb) @ T_sd


    T_sb = np.asarray( T_sb, dtype = float )
    T_sd = np.asarray( T_sd, dtype = float )


    return np.linalg.inv( T_sb ) @ T_sd
```

A building block that records a robust alternative:

```python
def compute_kalman_gain( P : np.ndarray, H : np.ndarray, R : np.ndarray ) -> np.ndarray:
    """
    Compute the Kalman gain.

    Parameters
    ----------
    P : array_like, shape (n, n)
        Predicted state covariance.
    H : array_like, shape (m, n)
        Measurement matrix.
    R : array_like, shape (m, m)
        Measurement noise covariance.

    Returns
    -------
    ndarray, shape (n, m)
        Kalman gain.
    """


    # pseudo-code:
    # S = H @ P @ H.T + R
    # K = P @ H.T @ inverse(S)
    #
    # robust alternative: K = solve( S.T, ( P @ H.T ).T ).T


    P = np.asarray( P, dtype = float )
    H = np.asarray( H, dtype = float )
    R = np.asarray( R, dtype = float )


    return P @ H.T @ np.linalg.inv( H @ P @ H.T + R )
```

### 3.3 Higher-level example

```python
def update_state_estimate( x_predicted : np.ndarray, P_predicted : np.ndarray, z : np.ndarray, H : np.ndarray, R : np.ndarray ) -> tuple[ np.ndarray, np.ndarray ]:
    """
    Fold one measurement into a predicted state (Kalman measurement update).

    Parameters
    ----------
    x_predicted : array_like, shape (n,)
        Predicted state.
    P_predicted : array_like, shape (n, n)
        Predicted state covariance.
    z : array_like, shape (m,)
        Measurement.
    H : array_like, shape (m, n)
        Measurement matrix.
    R : array_like, shape (m, m)
        Measurement noise covariance.

    Returns
    -------
    tuple of ndarray
        Updated state, shape (n,), and updated covariance, shape (n, n).
    """


    # pseudo-code:
    # K = compute_kalman_gain( P, H, R )
    # x = x_predicted + K @ ( z - H @ x_predicted )
    # P = ( I - K @ H ) @ P_predicted


    K = compute_kalman_gain( P_predicted, H, R )


    x_updated = compute_updated_state( x_predicted, K, z, H )
    P_updated = compute_updated_covariance( P_predicted, K, H )


    return x_updated, P_updated
```

### 3.4 Hybrid example

```python
def compute_point_in_space_frame( T_sb : np.ndarray, p_b : np.ndarray ) -> np.ndarray:
    """
    Express a point given in the body frame in the space frame.

    Parameters
    ----------
    T_sb : array_like, shape (4, 4)
        Pose of the body frame in the space frame.
    p_b : array_like, shape (3,)
        Point in the body frame.

    Returns
    -------
    ndarray, shape (3,)
        Point in the space frame.
    """


    # pseudo-code:
    # p_s = ( T_sb @ [ p_b ; 1 ] )[ 0 : 3 ]


    p_b_homogeneous = get_homogeneous_point( p_b )


    return ( T_sb @ p_b_homogeneous )[ : 3 ]
```

### 3.5 Plotting

For plotting, supply titles, labels, units, and legend text as arguments; construct displayed text with f-strings. Compose custom figures from the reusable `plot_` blocks inside a `visualize_` function.

---

## 4. C++ format

The C++ format mirrors the Python format as closely as the language allows, so that code in either language reads the same way. The same function types (section 1) and prefixes (section 2) apply, and names remain `snake_case`, so a function has the same name in both languages.

**Declarations and types**

- Use Eigen for vectors and matrices. Place all `#include` directives at the top of the file, and never use `using namespace`. Declare in a header (`#pragma once`) and define in a source file only when the project requires it; otherwise keep the function in a single header.
- A function defined in a header must be marked `inline`, otherwise including the header from two source files breaks the one-definition rule.
- Write the type of every parameter and the return type explicitly. Pass matrices by `const` reference and return by value. Use `double` precision. Avoid `auto` except where a type is genuinely unwieldy.

**Documentation**

- Document each function with a Doxygen block (`/** ... */`) directly above the signature, with no empty line between the block and the signature: a concise summary, then `@param` for every parameter and `@return`, stating shape, physical units, and conventions. This takes the place of the Python docstring.
- When a function is declared in a header and defined in a source file, the block belongs on the declaration in the header, because that is where hover documentation is read from. The definition may omit it.

**Body layout**

- Inside the body, leave **two empty lines** after the opening brace, then the concise mathematical pseudocode in `//` comments, then **two empty lines**, then the implementation. Leave **two empty lines** between logical implementation groups and before the final `return`.
- Use four-space indentation, with spaces inside parentheses and around operators (`compute_body_error_transform( T_sb, T_sd )`), and align declarations vertically as much as possible.
- Implement the mathematics directly. An explicit `.inverse()` is preferred to a solve, with the robust alternative recorded in the pseudocode comment (`// robust alternative: A.partialPivLu().solve( b )`). A plain `for` loop is acceptable where the pseudocode is a sum or product over an index.

**Checks, comments, and helpers**

- Do not add assertions, checks, or exceptions unless explicitly requested.
- Preserve existing comments and the blank-line rules of section 3.1 (print statements, comments, and doc blocks are separated from adjacent code by at least one empty line).
- Do not create additional helpers unless requested.

```cpp
#include <Eigen/Dense>


/**
 * Compute the body-frame error transform.
 *
 * @param T_sb  Current end-effector pose, shape (4, 4).
 * @param T_sd  Desired end-effector pose, shape (4, 4).
 * @return      Body-frame error transform, shape (4, 4).
 */
inline Eigen::Matrix4d compute_body_error_transform( const Eigen::Matrix4d & T_sb,
                                                     const Eigen::Matrix4d & T_sd )
{


    // pseudo-code:
    // T_bd = inverse( T_sb ) * T_sd
    //
    // robust alternative: T_sb.partialPivLu().solve( T_sd )


    return T_sb.inverse() * T_sd;
}
```

### 4.1 Classes and program structure

**One class per object**

- Each thing in the program (a robot, a sensor, a board, a ball) is its own class, named in `CamelCase` (as in section 6.2.1),
  declared in a `.hpp` and defined in a `.cpp`. Member functions are `snake_case` and use the prefixes of section 2.
- Classes for similar things share the same interface, so they are used the same way: `get_position()`, `set_position( ... )`,
  `update_position( dt )`, `render_shape( window )`.
- A getter's name states exactly what it returns: `get_bounds()` returns a bounding box, `get_position()` a position.

**Data and access**

- Plain data with no behavior is a `struct` with public members, for example `struct PlayerStats { int strength; int defense; };`.
- A class with behavior keeps its data `private` and exposes it only through `get_` and `set_` member functions (section 2.1).
- Values that describe one thing are grouped, in a `struct` or, for numeric quantities, an Eigen vector
  (`Eigen::Vector2d position`, not separate `x` and `y`). Numbered variables (`player1_strength`, `player2_strength`) and parallel
  arrays indexed by the same counter (`origin_x[ i ]`, `distance_x[ i ]`) are not used; use a `std::vector` of the struct.

**References between objects**

- An object that needs a shared object (the board, the game, the map) receives a pointer to it in its constructor and keeps it,
  instead of using a global. The pointer is named after the object (`board`, `game`, `map`) and is non-owning: it is never deleted
  through it.
- The owner holds objects by value, in a `std::vector`, or in a `std::unique_ptr`. Raw `new` and `delete` are not used.

**Base classes**

- Classes that share an interface (players and enemies, items and weapons) derive from an abstract base class that declares it
  with pure virtual functions, `virtual void apply_attack( Character & other ) = 0;`. Each implementation is marked `override`,
  and the base class has a `virtual` destructor.
- Constructor parameters have the same names as the members they set, assigned with `this->name = name;`. A default constructor
  delegates to the full one with default values: `Character() : Character( "Default Name", "Default Description" ) {}`.

**Program structure**

- `main()` is a higher-level function (section 1.2): it creates the objects, then runs the loop. Each loop iteration is the same
  fixed sequence, marked with section comments:

  ```cpp
  // setup: create objects
  while ( window.isOpen() )
  {
      // events
      // update
      // draw: clear, draw, display
  }
  ```

  Each stage may be a call, `update_game( ... )` and `render_scene( ... )`, with the loop itself in a `run_` function.

**Comments**

- Comments that explain from first principles are encouraged (section 7). When a value is changed, a short inline note may record
  the previous value and the reason (`// was <= (testing)`). Commented-out experiments may remain during development; only the
  owner removes them.

### 4.2 Optional conventions

- **State machines.** A program with modes or screens uses `enum class GameState { menu, playing, game_over };` and a `switch` in
  the main loop, instead of several `bool` flags combined in conditions. `bool` flags are kept for independent yes/no facts.
- **Range-based `for`** (`for ( const Ball & ball : balls )`) where the pseudocode iterates over the elements, and an indexed `for`
  where it uses the index.
- **The conditional operator** `?:` for a single two-way choice. A chain of several becomes `if` / `else if`.
- **Overloading** (one name for several parameter types) only where no Python counterpart must share the name, since Python has none.
- **`std::this_thread::sleep_for`** for pauses in console programs only, never in a real-time or ROS loop (use timers there).

---

## 5. Keeping IntelliSense working

The formats above are designed so that hover documentation, signature help, and completion work in both languages. The two languages place the documentation differently, but each placement is the one its tooling reads.

| Requirement | Python | C++ |
|---|---|---|
| Where the documentation lives | The docstring is the first statement in the function body, directly after the `def` line. Nothing, not even a comment, comes before it. | The Doxygen block sits directly above the declaration, with no empty line between. |
| Parameter and return types | Every parameter and the return type annotated. Use `tuple[ np.ndarray, np.ndarray ]`, never a bare `tuple`, so the element types are visible (Python 3.9 or later; otherwise `typing.Tuple`). | Explicit types in the signature. Fixed-size Eigen types (`Eigen::Matrix4d`) carry the shape in the type, while dynamic sizes use `Eigen::MatrixXd` and state the shape in `@param`. |
| Shapes and units | Stated in the docstring, because `np.ndarray` does not carry them. | Stated in `@param` and `@return` when the size is dynamic. |
| Local types | Annotated parameters keep their declared type after `np.asarray` rebinding. | Avoid `auto` for Eigen results, since it captures an expression template instead of a matrix, which breaks completion and can leave a dangling reference. |
| Imports and includes | At the top of the file, never inside a function, and no wildcard imports. | `#include` at the top, with no `using namespace`, so completion lists show qualified names. |
| Pseudocode and spacing | Placed after the docstring, so it never interferes with it. | Placed inside the body after the opening brace, so it never interferes with the block above the signature. |

Keep the Python and C++ names, parameter order, and docstring wording identical for the same function, so that hovering in either language gives the same description.

---

## 6. ROS 2

Follow the official ROS 2 conventions and the documentation of one pinned distribution (for example Humble or Jazzy) for the whole project. Check every example against that distribution, because behavior and style guidance differ between releases.

> **Precedence.** Where a ROS convention and the formatting rules of sections 3 and 4 conflict, the formatting rules govern code layout, and the ROS convention governs ROS names, interfaces, and structure.

### 6.1 Architecture: a thin ROS layer over a pure core

- Building blocks and higher-level functions (sections 1 to 4) never import or include ROS. They contain no `rclpy`, `rclcpp`, message types, clocks, or loggers, and they operate on plain arrays and Eigen matrices. This keeps them testable without ROS, identical in simulation, on a bag file, and on hardware, and faithful to the pseudocode.
- A node is a thin wrapper. A callback converts the incoming message to arrays with a `get_` function, calls the core functions, and publishes the result. A callback contains no mathematics.
- Callbacks stay short and non-blocking. Heavy computation runs on a timer or its own callback group, never inside a subscription callback that must keep up with a sensor.
- One node has one responsibility, matching one block of the data flow diagram.

### 6.2 Packages and files

#### 6.2.1 Layout, naming, and language

One package per cohesive component. Keep the core library, the node wrappers, the launch files, the parameter files, and the tests in clearly separated directories:

```
package_name/
    package.xml
    setup.py or CMakeLists.txt
    package_name/        core functions (Python) or include/ and src/ (C++)
    launch/
    config/
    test/
```

- Packages, nodes, topics, services, actions, parameters, and frame names are lowercase `snake_case`. Class names are `CamelCase`, as the ROS 2 style guide specifies. C++ headers use `.hpp` and sources use `.cpp`.
- Target the language versions of the pinned distribution (C++17 for current releases, and Python 3). Keep lines within 100 characters where the formatting rules of sections 3 and 4 allow.
- Put custom messages, services, and actions in a separate `package_name_interfaces` package, and use standard types (`geometry_msgs`, `sensor_msgs`, `nav_msgs`, `tf2_msgs`) before defining new ones.

#### 6.2.2 Package types

Choose one of three package types, according to the languages the package contains. Prefer a single-language package, and use the mixed type only when one package genuinely needs both languages and the two parts are tightly coupled. Otherwise split the languages into separate packages.

| | Pure Python | Pure C++ | Mixed Python and C++ |
|---|---|---|---|
| Create | `ros2 pkg create package_name --build-type ament_python` | `ros2 pkg create package_name --build-type ament_cmake` | `ros2 pkg create package_name --build-type ament_cmake` |
| Build files | `setup.py` and `setup.cfg` | `CMakeLists.txt` | `CMakeLists.txt` only (no `setup.py` or `setup.cfg`) |
| Buildtool dependency | `ament_python` | `ament_cmake` | `ament_cmake` and `ament_cmake_python` |
| Client library | `rclpy` | `rclcpp` | `rclcpp` and `rclpy` |
| Executables | `console_scripts` entry points | `install( TARGETS ... DESTINATION lib/${PROJECT_NAME} )` | C++ with `install( TARGETS ... )`, Python scripts with `install( PROGRAMS ... )` |

C++ headers go in `include/package_name/` and sources in `src/`.

#### 6.2.3 Mixed Python and C++ packages

This follows [the Robotics Backend guide](https://roboticsbackend.com/ros2-package-for-both-python-and-cpp-nodes/). Create the package with `--build-type ament_cmake` and let CMake install the Python parts through `ament_cmake_python`. Use `ament_cmake_python`, not `ament_python`. The layout is:

```
package_name/
    CMakeLists.txt
    package.xml
    package_name/            Python module folder, with an empty __init__.py
        __init__.py
        core.py
    scripts/                 Python node scripts
        py_node.py
    include/package_name/    C++ headers
    src/                     C++ sources, each node with a main()
```

In `package.xml`, declare both build tools and both client libraries:

```xml
<buildtool_depend>ament_cmake</buildtool_depend>
<buildtool_depend>ament_cmake_python</buildtool_depend>
<depend>rclcpp</depend>
<depend>rclpy</depend>
```

In `CMakeLists.txt`, add the following before `ament_package()`:

```cmake
find_package( ament_cmake_python REQUIRED )
find_package( rclcpp REQUIRED )
find_package( rclpy REQUIRED )

include_directories( include )
add_executable( cpp_executable src/cpp_node.cpp )
ament_target_dependencies( cpp_executable rclcpp )
install( TARGETS cpp_executable DESTINATION lib/${PROJECT_NAME} )

ament_python_install_package( ${PROJECT_NAME} )
install( PROGRAMS scripts/py_node.py DESTINATION lib/${PROJECT_NAME} )
```

- Each Python script begins with `#!/usr/bin/env python3`, without which `ros2 run` and launch files fail, and is made executable with `chmod +x`. The executable bit is required when building with `--symlink-install`, otherwise `ros2 run` reports that no executable was found.
- The core library in the module folder is then imported as `from package_name.core import ...`.
- Build with `colcon build --packages-select package_name`, and run with `ros2 run package_name cpp_executable` and `ros2 run package_name py_node.py`. The Python scripts are launched with their `.py` extension in this layout.

#### 6.2.4 `package.xml` and `rosdep`

Populate `package.xml` completely, so that `rosdep` can install every dependency on a clean machine with `rosdep install --from-paths src --ignore-src -r -y`.

- Use package format 3, and fill in the name, version, description, maintainer, and license.
- Declare every package the code imports or includes, in the correct tag:

| Tag | Use |
|---|---|
| `<buildtool_depend>` | The build tool (`ament_cmake`, `ament_python`). |
| `<build_depend>` | Compile-time needs. |
| `<exec_depend>` | Run-time needs. |
| `<depend>` | Needed for both. This is the usual case for `rclcpp`, message packages, and `eigen`. |
| `<test_depend>` | Test-only packages (`ament_lint_auto`, `ament_lint_common`, `python3-pytest`). |

- Include `<export><build_type>` so the build system is identified.
- Use `rosdep` keys, not pip or CMake names. For example, use `python3-numpy` for NumPy and `eigen` for Eigen. A dependency written under a name that `rosdep` cannot resolve is skipped silently or fails the install.
- Check each key before committing it with `rosdep resolve <key>`, and run `rosdep check --from-paths src --ignore-src` after editing. Do the same for Pinocchio and any other library, since the key and its availability depend on the distribution.
- Avoid a dependency that is only available through pip. If one is unavoidable, add a custom `rosdep` rule for it and document it in the repository, so the install stays one command.
- Keep `package.xml` in step with the code. A new `import` or `#include` of a package means a new dependency tag in the same commit.

### 6.3 Frames, units, and time

- Follow REP 103 (standard units and coordinate conventions): SI units throughout (meters, radians, seconds, kilograms), right-handed frames, and body frames with x forward, y left, and z up. Camera optical frames use z forward, x right, and y down. State the frame in every docstring or `@param`.
- Follow [REP 105](https://reps.openrobotics.org/rep-0105/) for the frame tree: `map` is the parent of `odom`, and `odom` is the parent of `base_link`. A frame has exactly one parent. The `odom` frame is continuous but drifts, so use it for local quantities such as the elevation map. The `map` frame can jump, and the global estimator publishes `map` to `odom`, while the odometry source publishes `odom` to `base_link`. Every sensor frame needs a transform to `base_link`.
- Every stamped message carries a correct `header.stamp` and `header.frame_id`. Fill covariance fields (for example the 6 by 6 pose covariance) honestly, because downstream filters and maps consume them.
- Use the node clock (`self.get_clock().now()` in Python, `this->now()` in C++), never the wall clock, so that simulation and bag replay work. Every node exposes `use_sim_time`. Record with `ros2 bag record` and replay with `ros2 bag play --clock`. Synchronize sensor streams by message timestamp (`message_filters`), not by arrival order.

### 6.4 Quality of service

- Set the quality of service explicitly on every publisher and subscriber. Do not rely on defaults.
- Sensor streams use the sensor-data profile: best effort reliability, keep last history, volatile durability. A best-effort publisher does not match a reliable subscriber, so subscribe to sensor topics with a compatible profile. In Python this is `qos_profile_sensor_data`, and in C++ it is `rclcpp::SensorDataQoS()`.
- Commands, waypoints, and other messages that must not be lost use reliable reliability. Data that late joiners need (a static map, a goal) uses transient local durability.
- Choose the history depth from the sensor rate and how many frames can be dropped, and state the reason in a comment.

### 6.5 Parameters, launch, and configuration

- Declare every parameter with a default value and a descriptor (description, type, and range where meaningful). Read parameters once at startup, and use a parameter callback only when a value must change at runtime. No magic numbers in a node.
- Keep parameter values in YAML files in `config/`, loaded by the launch file, and not in the code.
- Write launch files in Python. Declare launch arguments for everything a user may change (`use_sim_time`, configuration file path, namespace), and use remappings and namespaces. Topic names in code are relative, with no leading slash, so a node can be remapped without editing it.

### 6.6 Composable and lifecycle nodes

Write nodes as composable nodes wherever possible, and migrate existing standalone-executable nodes to composable nodes when possible.

- A composable node is a class derived from `rclcpp::Node` whose constructor takes `const rclcpp::NodeOptions &`, registered with `RCLCPP_COMPONENTS_REGISTER_NODE` and built as a shared library through `rclcpp_components` (add `rclcpp_components` to `package.xml`). In `CMakeLists.txt` this is `rclcpp_components_register_node( target PLUGIN "package_name::NodeClass" EXECUTABLE node_name )`.
- Launch such nodes in a component container with `ComposableNodeContainer` and `ComposableNode`.
- Several composable nodes in one container communicate without serialization or copying when intra-process communication is enabled and messages are published as `std::unique_ptr`. This matters most for high-bandwidth streams such as point clouds and images.
- The standalone executable remains a thin `main()` that instantiates the same class, for debugging.
- Composition is a C++ capability. To my knowledge, a Python node cannot be loaded as a composable node in the Humble and Jazzy releases, so put high-bandwidth and latency-sensitive nodes in C++ as composable nodes, and keep Python for prototypes and low-rate nodes. Verify this against the pinned distribution.
- Use lifecycle nodes for components that need an explicit configure and activate sequence.

### 6.7 Logging

- Use the node logger and never `print` inside a node: `self.get_logger().info( f"..." )` in Python and `RCLCPP_INFO( get_logger(), "..." )` in C++. Use the throttled variants for messages in high-rate callbacks.
- The blank-line and f-string rules of section 3.1 apply to logger calls as they do to `print`. Standalone scripts outside nodes still use `print`.

### 6.8 Node example

```python
import numpy as np
import rclpy
from rcl_interfaces.msg import ParameterDescriptor
from rclpy.node import Node
from geometry_msgs.msg import PoseStamped

from package_name.core import compute_body_error_transform, get_transform_from_pose_message


class BodyErrorNode( Node ):

    def __init__( self ) -> None:
        super().__init__( "body_error_node" )


        self.declare_parameter( "desired_pose_topic", "desired_pose", ParameterDescriptor( description = "Topic carrying the desired end-effector pose." ) )
        self.declare_parameter( "current_pose_topic", "current_pose", ParameterDescriptor( description = "Topic carrying the current end-effector pose." ) )


        self.T_sd = None


        self.create_subscription( PoseStamped, self.get_parameter( "desired_pose_topic" ).value, self.callback_desired_pose, 10 )
        self.create_subscription( PoseStamped, self.get_parameter( "current_pose_topic" ).value, self.callback_current_pose, 10 )


    def callback_desired_pose( self, message : PoseStamped ) -> None:
        self.T_sd = get_transform_from_pose_message( message )


    def callback_current_pose( self, message : PoseStamped ) -> None:
        if self.T_sd is None:
            return


        T_sb = get_transform_from_pose_message( message )
        T_bd = compute_body_error_transform( T_sb, self.T_sd )


        self.get_logger().info( f"Body error translation norm: {np.linalg.norm( T_bd[ : 3, 3 ] ):.3f} m" )
```

The mathematics lives in `package_name.core`, which has no ROS imports. The node only converts, calls, and logs.

### 6.9 Build, test, and lint

- Build with `colcon` using `ament_python` or `ament_cmake`. Add `ament_lint_auto` to the test dependencies.
- Unit-test every building block directly (`pytest` for Python, `gtest` for C++), with no ROS running. Test nodes separately with launch tests.
- The default linters (`ament_flake8`, `ament_cpplint`, `ament_uncrustify`) will reject this project's layout: spaces inside parentheses, aligned columns, spaces around keyword equals, and two blank lines inside a function. Do not reformat the code to satisfy them. Run each linter once and configure it to ignore only the specific rules it reports for this style (for `flake8` these are likely `E201`, `E202`, `E203`, `E221`, `E222`, `E241`, `E251`, `E303`, and possibly `E501`), and keep every other rule enabled.

### 6.10 References

- [ROS 2 code style and language versions](https://docs.ros.org/en/jazzy/The-ROS2-Project/Contributing/Code-Style-Language-Versions.html)
- [REP 105](https://reps.openrobotics.org/rep-0105/)
- [ROS 2 quality of service settings](https://docs.ros.org/en/jazzy/Concepts/Intermediate/About-Quality-of-Service-Settings.html)
- [Robotics Backend guide to mixed Python and C++ packages](https://roboticsbackend.com/ros2-package-for-both-python-and-cpp-nodes/)
- [Nav2 transform setup guide](https://docs.nav2.org/setup_guides/transformation/setup_transforms.html)

---

## 7. Rules for AI agents working in this code

- Keep the variable names in code I provide exactly as written, in my naming style. Do not simplify, shorten, or change them.
- When suggesting code, give the logic or formula and simplified pseudocode alongside it.
- Derive explanations and comments from first principles unless told otherwise.
- Do not create helper functions unless directed.
- Write code to the building block conventions above: direct, matching the pseudocode, with robustness and efficiency deferred.
- Use full words rather than abbreviations in names, follow best practices within these constraints, and keep a professional, academic tone. Do not use emojis or slang.
