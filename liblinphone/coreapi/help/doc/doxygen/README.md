# Linphone Configuration Keys Documentation Guide

This document explains the structure, formatting conventions, and maintenance guidelines for [`provisioning_configuration_key.dox.in`](provisioning_configuration_key.dox.in).

---

## 1. Overview & Purpose

[`provisioning_configuration_key.dox.in`](provisioning_configuration_key.dox.in) is the single source of truth for documenting the INI-style `linphonerc` configuration file parameters. It serves two main purposes:
1. **Remote Provisioning Reference**: Outlines keys supported in XML / remote provisioning profiles served via HTTP(S) (e.g. through Flexisip Account Manager).
2. **API & Engine Reference**: Documents available tuning parameters across audio, video, network, SIP signaling, chat, and account management.

During the build process, CMake preprocesses this file via `sed` (using rules defined in [`cmake_doxygen_sed_fixes.txt`](cmake_doxygen_sed_fixes.txt)) and feeds it into Doxygen to generate:
- The C Doxygen documentation page: `Provisioning configuration keys` (`group_provisioning_configuration_key`).
- Wrapper documentation artifacts for C# (which renders the full tables) and Swift, Java, Python (which use [`simple_provisioning_configuration_key.dox.in`](simple_provisioning_configuration_key.dox.in)).

---

## 2. Parameter Classification: Public vs. Private

Every configuration parameter must be categorized into one of two tables per section:

| Category | Definition | Presentation | Columns |
| :--- | :--- | :--- | :--- |
| **Public Parameters** | Keys that have an official programmatic getter / setter in the public Liblinphone C API. | Main visible table | `Key`, `Value type`, `Possible values`, `Default value`, `API`, `Description` (via `\copybrief`) |
| **Private Parameters** | Internal parameters, test-only keys, debugging flags, or keys not yet exposed through the public API. | Collapsible `<details>` block (hidden by default) | `Key`, `Value type`, `Possible values`, `Default value`, `Description` |

> [!IMPORTANT]
> Always check whether a public `linphone_core_*` or object-specific API function exists before adding a key. If no public C API function exists, it **must** go into the private table.

---

## 3. Table Structure and Column Definitions

### 3.1 Public Parameters Table

```html
<table>
	<caption id="<section>_parameters"><Section Name> parameters</caption>
	<tr><th>Key<th>Value type<th>Possible values<th>Default value<th>API</th>
	\if DOXYFILE_USE_COPYBRIEF
		<th>Description</th>
	\endif
	</tr>
	<tr><td>key_name</td><td>TYPE</td><td>POSSIBLE_VALUES</td><td>DEFAULT</td><td>linphone_core_set_feature()</td><td>\copybrief linphone_core_set_feature</td></tr>
</table>
```

* **Key**: The exact case-sensitive key name as read via `linphone_config_get_*()`.
* **Value type**: `integer`, `boolean`, `string`, `float`, or `range`.
* **Possible values**: Range or possible options (e.g. `0 or 1`, `-2 - 65535`, `qcif, cif, vga, 720p`).
* **Default value**: The fallback value if unset in `linphonerc`.
* **API**: The public Liblinphone C API function name (e.g. `linphone_core_enable_video_capture()`).
* **Description**: Must use `<td>\copybrief <api_function></td>`. Doxygen pulls the brief doc directly from the C header.

### 3.2 Private Parameters Table

```html
\if DOXYFILE_USE_DETAILS
<details>
	<summary>Show private variables</summary>
	<table>
		<caption id="private_<section>_parameters">Private <Section Name> parameters</caption>
		<tr><th>Key<th>Value type<th>Possible values<th>Default value<th>Description</th></tr>
		<tr><td>key_name</td><td>TYPE</td><td>POSSIBLE_VALUES</td><td>DEFAULT</td><td>A clear and concise description of the internal behavior.</td></tr>
	</table>
</details>
\endif
```

* **No `API` column**: Private keys do not have an API column.
* **Direct description**: The `Description` column contains an explicit text description rather than `\copybrief`.

---

## 4. Syntax Constraints & Compatibility Rules

Because this document is processed by multiple tools (CMake, sed, Doxygen, and language wrapper generators), follow these mandatory rules:

1. **`\copybrief` Placement**:
   - Write `<td>\copybrief function_name</td>` on a single cell without extra HTML tags inside.
   - The regex in [`cmake_doxygen_sed_fixes.txt`](cmake_doxygen_sed_fixes.txt) expects `\copybrief` to be immediately followed by the function name and enclosed by `<td>...</td>`.

2. **Doxygen Conditionals**:
   - Always wrap the public description header in `\if DOXYFILE_USE_COPYBRIEF ... \endif`.
   - Always wrap the private `<details>` block in `\if DOXYFILE_USE_DETAILS ... \endif`.
   - These flags ensure compatibility with older Doxygen versions (< 1.9) that do not support `<details>` or in-cell copybrief.

3. **Section Headings & Captions**:
   - Section heading format:
     ```html
     <h2>Section Title - section [<i>section_name</i>]</h2>
     ```
   - For indexed sections (accounts, friend lists, etc.), use the `_N` notation:
     ```html
     <h2>SIP accounts - section [<i>proxy_N</i>]</h2>
     ```
   - Always provide unique `id` attributes for captions (`<caption id="sip_parameters">`, `<caption id="private_sip_parameters">`).

4. **Alphabetical Ordering**:
   - Keep keys within each table sorted alphabetically to simplify search and maintenance.

---

## 5. Examples

### Adding a Public Key

```html
<tr><td>playback_gain_db</td><td>float</td><td>-30 to +30 (dB)</td><td>0</td><td>linphone_core_set_playback_gain_db()</td><td>\copybrief linphone_core_set_playback_gain_db</td></tr>
```

### Adding a Private Key

```html
<tr><td>register_retry_base_delay_ms</td><td>integer</td><td>Milliseconds</td><td>0</td><td>Base delay in milliseconds for the exponential retry backoff with jitter for REGISTER. When set to 0, exponential backoff is disabled and fixed retry delay is used.</td></tr>
```

### Adding a New Section

```html
<h2>Storage - section [<i>storage</i>]</h2>
The <i>storage</i> section represents configuration related to local database storage.
<table>
	<caption id="storage_parameters">Storage parameters</caption>
	<tr><th>Key<th>Value type<th>Possible values<th>Default value<th>API</th>
	\if DOXYFILE_USE_COPYBRIEF
		<th>Description</th>
	\endif
	</tr>
	<tr><td>update_db_at_initialisation</td><td>boolean</td><td>0 or 1</td><td>1</td><td>linphone_core_enable_update_db_at_startup()</td><td>\copybrief linphone_core_enable_update_db_at_startup</td></tr>
</table>
```

---

## 6. How to Build and Validate Documentation

To generate and verify the documentation locally:

1. Enable documentation in your CMake configuration:
   ```bash
   cmake -B build -DENABLE_DOC=YES [other options...]
   ```

2. Build the documentation target:
   ```bash
   cmake --build build --target liblinphone-doc
   ```

3. Open the generated HTML page in your browser:
   ```bash
   open build/liblinphone/coreapi/help/doc/doxygen/c/group__group__provisioning__configuration__key.html
   ```

Verify that:
- Tables render with correct alignment and styling.
- The `<details>` sections expand properly.
- All `\copybrief` tags expand into their corresponding function descriptions without broken links.

---

## 7. Role of `simple_provisioning_configuration_key.dox.in`

In the same directory, [`simple_provisioning_configuration_key.dox.in`](simple_provisioning_configuration_key.dox.in) acts as a lightweight **stub / relay** documentation page for language wrappers.

* **Usage**: Used by Swift, Python, and Java wrappers (`liblinphone/wrappers/{swift,python,java}/CMakeLists.txt`).
* **Purpose**: Instead of duplicating and maintaining huge HTML tables across all language wrappers (which caused regex transformation and parsing maintenance issues), these wrappers generate a short introductory page that redirects developers directly to the authoritative online C documentation (`https://download.linphone.org/.../group__group__provisioning__configuration__key.html`).
* **Full Tables**: The complete configuration tables from `provisioning_configuration_key.dox.in` are rendered directly in the **C Doxygen documentation** and in the **C# wrapper documentation**.

> [!WARNING]
> Do **not** delete `simple_provisioning_configuration_key.dox.in`. Doing so will break CMake configuration and documentation builds for Swift, Python, and Java wrappers.

