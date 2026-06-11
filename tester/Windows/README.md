# Linphone Tester for UWP

## Overview

This repository contains a Visual Studio 2022 solution used to run and automate Linphone SDK unit tests on the Universal Windows Platform (UWP).

The solution consists of two projects:

* **LinphoneTester_UWP** – A UWP application that hosts and executes Linphone test suites.
* **LinphoneTester_Server** – A .NET Framework 4.5.2 console application used to launch and control UWP test executions.

---

## Prerequisites

### Visual Studio

The solution must be built using **Visual Studio 2022**.

### Linphone SDK

Before building the solution, the Linphone SDK must be compiled with unit test support enabled:

```text
ENABLE_UNIT_TESTS=ON
```

Once built, install the SDK into a local directory that will be referenced by the Visual Studio projects.

---

## Solution Architecture

### UWP Client

The **LinphoneTester_UWP** project generates the multi-instance UWP application responsible for executing Linphone test suites.

Since UWP applications run within the Windows application model, they cannot be launched directly from their executable file. A UWP package must first be installed on the system and then launched through the Windows application framework.

---

### Server Application

The **LinphoneTester_Server** project is a **.NET Framework 4.5.2 console application** that acts as a command dispatcher for test execution.

The server listens on:

```text
localhost:50000
```

and accepts incoming test commands.

Each received command is executed in a separate process, allowing tests to run independently and in parallel when required.

The server exists specifically to overcome UWP execution restrictions by launching UWP test instances instead of the same application and with the appropriate activation context.

---

## UWP Execution Constraints and Test Architecture

The test framework relies on **BCToolBox** to orchestrate and execute unit test suites. However, BCToolBox is a **Win32-native library** and is not designed to operate within a pure UWP execution environment.

This introduces a fundamental limitation: **direct parallel execution of tests using BCToolBox is incompatible with UWP application lifecycle rules.**

---

### UWP Runtime Limitation

When tests are executed in parallel, BCToolBox spawns each test suite using `CreateProcess`.

This is problematic in a UWP context because:

* `CreateProcess` starts a raw executable without going through the Windows App Activation Broker.
* The Windows broker is responsible for:

  * Creating the `CoreApplication` instance
  * Initializing the XAML UI thread
  * Dispatching `OnLaunched` / `OnActivated` events
  * Managing the UWP application lifecycle

As a result, processes started via `CreateProcess` in this context:

* do not properly initialize a UWP UI environment
* may not create a valid XAML window
* do not reliably enter the application event loop
* may execute only partial application initialization (e.g. constructors only)

---

### .NET Server Bridge

To overcome this limitation, a **.NET Framework server layer** is introduced as an intermediary execution broker.

Instead of allowing BCToolBox to directly spawn UWP processes, all execution requests are routed through this server, which:

* receives test execution commands
* reissues them using a UWP-compatible launch mechanism
* ensures that each execution runs inside a valid UWP activation context

This approach guarantees that every test instance is properly initialized by the Windows application model.

---

## UWP Test Execution Flow

The UWP test runner supports two execution modes depending on the provided parameters.

---

### 1. Standard Execution Mode (Single Suite / Single Test)

If the command-line arguments define a specific test or suite (for example a single test case or a specific test suite), then the test is executed normally inside the current UWP instance.

In this mode:

* the application behaves like a standard UWP test runner
* lifecycle events (`OnLaunched`, `OnActivated`) are fully executed
* results are returned directly to the caller

---

### 2. Parallel / Delegated Execution Mode


When the command-line arguments request parallel execution without explicitly specifying a test suite or an individual test case, the system switches to **delegated execution mode**.

In this mode, the UWP tester appends the `--delegate` flag, allowing BCToolBox to continue generating execution requests through its native `CreateProcess` mechanism. However, these requests are not executed directly as raw processes. Instead, they are intercepted and forwarded to the .NET server layer.

The server is responsible for relaunching each request using a **UWP-compatible activation mechanism**, ensuring that the application is started through the Windows App Activation Broker. This guarantees proper initialization of the UWP runtime, including the XAML UI thread and all application lifecycle events.

Each test execution is therefore isolated in its own fully initialized UWP process.

---

## Building the Solution

Before building, verify that the Linphone SDK path is correctly configured.

### Configure LinphoneSDK-Path

1. Open Visual Studio.

2. Navigate to:

   ```text
   View → Other Windows → Property Manager
   ```

3. Expand the active configuration.

4. Open the property sheet named:

   ```text
   Linphone_<Type>_<Arch>
   ```

5. Open **Property Pages**.

6. Navigate to:

   ```text
   User Macros
   ```

7. Verify that the `LinphoneSDK-Path` macro points to your SDK installation.

Example:

```text
$(MSBuildProjectDirectory)\..\..\..\..\build\$(Configuration)\linphone-sdk\uwp-$(Platform)
```

If your SDK is installed elsewhere, update the path accordingly.

For example:

```text
C:\linphone-sdk\uwp
```

---

### Select the Target Platform

Set the Solution Platform to one of the supported architectures:

* x86
* x64

Alternatively, use **Configuration Manager** if you want to apply the platform selection only to the UWP client project.

---

## Common Build Issues

### Namespace Not Found Errors

If you encounter errors such as:

```text
The type or namespace name 'LinphoneTester' could not be found
```

verify that the `LinphoneSDK-Path` macro points to a valid SDK installation.

---

## Configuring Test Execution

Test arguments can be configured in Visual Studio:

```
LinphoneTester_UWP
→ Properties
→ Debugging
→ Command Line Arguments
```

Example:

```text
linphone_testerUWP.exe --suite "Setup" --verbose --parallel --parallel-max 10
linphone_testerUWP.exe --suite "Setup" --verbose --test "Version check" --keep
linphone_testerUWP.exe --verbose --parallel --parallel-max 10 --keep
```

---

## Additional UWP-Specific Arguments

### `--keep`

By default, the application closes automatically when test execution completes.

The `--keep` flag keeps the application running after the tests finish.

This is useful because some console output generated on Windows may not appear in Visual Studio's Output window. Keeping the application open allows developers to review the complete log output.

---

### `--delegate`

Indicates that the current process is launched by the server as part of a delegated test execution.

It is not intended to be used directly.

This flag is used to:

* isolate tests in separate UWP processes
* enable parallel execution scenarios
* ensure correct routing of results back to the server

---

## Updating UWP Capabilities

The repository provides a script named:

```
UpdatePermissions.bat
```

This script must be run after building the application. It updates Windows registry entries to grant the UWP application's publisher access to restricted capabilities such as:

```
phoneCalls
```

The script uses the package Publisher ID of the installed UWP application, retrieved using PowerShell:

```powershell
(Get-AppxPackage -Name "*LinphoneTester-UWP*").PublisherId
```

Once the Publisher ID has been obtained, the script applies the required registry modifications.

---
