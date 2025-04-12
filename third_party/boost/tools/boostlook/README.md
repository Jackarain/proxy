# BoostLook

This is a set of stylesheets, templates, and code for Asciidoctor and
Antora rendered documentation to give it a uniform look and feel
befitting the quality of Boost.

Example of integration into a doc Jamfile:
```
html mp11.html : mp11.adoc
    :   <use>/boost/boostlook//boostlook
        <dependency>mp11-docinfo-footer.html
    ;
```

Noto font files are covered under the Open Font License:

https://fonts.google.com/noto/use

## Live Preview for AsciiDoc Documentation

BoostLook includes a **live preview** feature to streamline local development and review of AsciiDoc documentation. This allows for automatic rendering and real-time browser preview of changes made to [`specimen.adoc`](/doc/specimen.adoc) and [`boostlook.css`](/boostlook.css) files.

### Overview

The preview functionality is handled by a Ruby script ([`boostlook_preview.rb`](./boostlook_preview.rb)). This script monitors AsciiDoc, HTML, and CSS files for changes, automatically rebuilding and opening the rendered HTML in your default web browser.

---

### Prerequisites

Ensure the following dependencies are installed:

- **Ruby** (>= 2.7 recommended)
- **Asciidoctor** – Install via `gem install asciidoctor`
- **Listen Gem** – Install via `gem install listen`
- **Boost.Build (b2)** – Required for builds invoked by the script. Ensure it is installed and available in your system's PATH.

---

### Usage and Detailed Steps

1. **Install Ruby and Required Gems**:
    - Ensure Ruby is installed on your system. You can check by running:
      ```bash
      ruby -v
      ```
      If Ruby is not installed, follow the instructions on [ruby-lang.org](https://www.ruby-lang.org/en/documentation/installation/) to install it.
    - Install Asciidoctor:
      ```bash
      gem install asciidoctor
      ```
    - Install the Listen gem:
      ```bash
      gem install listen
      ```

2. **Install Boost.Build (b2)**:
    - Follow the instructions on the [Boost.Build website](https://boostorg.github.io/build/) to install `b2`.
    - Ensure `b2` is available in your system's PATH by running:
      ```bash
      which b2
      ```

3. **Locate Specimen AsciiDoc File**:
    - Place your AsciiDoc files in the `doc` directory. The default file is `doc/specimen.adoc`.
    -  The default file structure should include:

    ```
     /boostlook
       ├── doc
       │   └── specimen.adoc <--
       │   └── ...
       ├── boostlook.css
       └── boostlook_preview.rb
       └── boostlook.rb
       └── ...
     ```

4. **Navigate to the project directory**:
    ```bash
    cd /path/to/boostlook
    ```

5. **Run the preview script**:
    - Ensure you're in the root folder
    ```bash
    ruby boostlook_preview.rb
    ```

6. **Edit and Preview**:
    - Edit your AsciiDoc (specimen.adoc) or CSS (boostlook.css) files.
    - The script will automatically detect changes and rebuild the documentation.
    - Refresh the browser to see changes.

7. **Refresh Your Browser** to view changes.

---

### Troubleshooting

- **Script Not Running**:
  - Ensure all prerequisites are installed correctly.
  - Check for any error messages in the terminal and resolve them accordingly.

- **Changes Not Reflecting**:
  - Ensure the files being edited are within the monitored directories (`doc` and root directory).
  - Verify that the browser is refreshing automatically or manually refresh the browser.

- **Boost.Build (b2) Not Found**:
  - Ensure `b2` is installed and available in your system's PATH.
  - Run `which b2` to verify its availability.
