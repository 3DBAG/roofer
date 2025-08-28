# Configuration file for the Sphinx documentation builder.
#
# For the full list of built-in configuration values, see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

# -- Project information -----------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#project-information

project = 'roofer'
copyright = '2024 Ravi Peters, Balazs Dukai and TU Delft 3D geoinformation group'
author = ''
release = '1.0.0-beta.5'

# -- General configuration ---------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#general-configuration

extensions = [
    'breathe',
    'sphinx.ext.autodoc',
    'myst_parser'
]

myst_enable_extensions = [
  "colon_fence",
  "html_admonition"
]

# make rooferpy module findable for autodoc
import sys, os
sys.path.insert(0, os.path.abspath('rooferpy'))

templates_path = ['_templates']
exclude_patterns = ['_build', 'Thumbs.db', '.DS_Store']

breathe_projects = {
    "Roofer": "./xml"
}
breathe_default_project = "Roofer"
# breathe_use_project_refid = True
# breathe_default_members = ('members', 'namespace')

# -- Options for HTML output -------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#options-for-html-output

html_theme = 'furo'
html_static_path = ['_static']

html_theme_options = {
    "light_css_variables": {
        "color-announcement-background": "var(--color-problematic)",
    },
    "dark_css_variables": {
        "color-announcement-background": "var(--color-problematic)",
    },
    "footer_icons": [
        {
            "name": "GitHub",
            "url": "https://github.com/3DBAG/roofer",
            "html": """
                <svg stroke="currentColor" fill="currentColor" stroke-width="0" viewBox="0 0 16 16">
                    <path fill-rule="evenodd" d="M8 0C3.58 0 0 3.58 0 8c0 3.54 2.29 6.53 5.47 7.59.4.07.55-.17.55-.38 0-.19-.01-.82-.01-1.49-2.01.37-2.53-.49-2.69-.94-.09-.23-.48-.94-.82-1.13-.28-.15-.68-.52-.01-.53.63-.01 1.08.58 1.23.82.72 1.21 1.87.87 2.33.66.07-.52.28-.87.51-1.07-1.78-.2-3.64-.89-3.64-3.95 0-.87.31-1.59.82-2.15-.08-.2-.36-1.02.08-2.12 0 0 .67-.21 2.2.82.64-.18 1.32-.27 2-.27.68 0 1.36.09 2 .27 1.53-1.04 2.2-.82 2.2-.82.44 1.1.16 1.92.08 2.12.51.56.82 1.27.82 2.15 0 3.07-1.87 3.75-3.65 3.95.29.25.54.73.54 1.48 0 1.07-.01 1.93-.01 2.2 0 .21.15.46.55.38A8.013 8.013 0 0 0 16 8c0-4.42-3.58-8-8-8z"></path>
                </svg>
            """,
            "class": "",
        },
    ],
    "source_repository": "https://github.com/3DBAG/roofer/",
    "source_branch": "develop",
    "source_directory": "docs/",
    # "announcement": "Roofer is still under active development.",
}

_GITHUB_ADMONITIONS = {
    "> [!NOTE]": "note",
    "> [!TIP]": "tip",
    "> [!IMPORTANT]": "important",
    "> [!WARNING]": "warning",
    "> [!CAUTION]": "caution",
}

# based on https://github.com/executablebooks/MyST-Parser/issues/845#issuecomment-2793118480
def run_convert_github_admonitions_to_rst_source(app, filename, lines):
    run_convert_github_admonitions_to_rst(app, filename, None, lines)

def run_convert_github_admonitions_to_rst(app, relative_path, parent_docname, lines):
    # loop through lines, replace github admonitions
    for i, orig_line in enumerate(lines):
        orig_line_splits = orig_line.split("\n")
        replacing = False
        for j, line in enumerate(orig_line_splits):
            # look for admonition key
            for admonition_key in _GITHUB_ADMONITIONS:
                if admonition_key in line:
                    line = line.replace(admonition_key, ":::{" + _GITHUB_ADMONITIONS[admonition_key] + "}\n")
                    # start replacing quotes in subsequent lines
                    replacing = True
                    break
            else:
                # replace indent to match directive
                if replacing and "> " in line:
                    line = line.replace("> ", "  ")
                elif replacing:
                    # missing "> ", so stop replacing and terminate directive
                    line = f"\n:::\n{line}"
                    replacing = False
            # swap line back in splits
            orig_line_splits[j] = line
        # swap line back in original
        lines[i] = "\n".join(orig_line_splits)


def setup(app):
    print("setupset")
    app.connect("include-read", run_convert_github_admonitions_to_rst)
    app.connect("source-read", run_convert_github_admonitions_to_rst_source)
