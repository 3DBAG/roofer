# Minimal makefile for Sphinx documentation
#

# You can set these variables from the command line, and also
# from the environment for the first two.
SPHINXOPTS    ?=
SPHINXBUILD   ?= sphinx-build
PYTHON        ?= python
SOURCEDIR     = .
BUILDDIR      = _build

# Put it first so that "make" without argument is like "make help".
help:
	@$(SPHINXBUILD) -M help "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)

.PHONY: help Makefile

# Catch-all target: route all unknown targets to Sphinx using the new
# "make mode" option.  $(O) is meant as a shortcut for $(SPHINXOPTS).
%: Makefile
	mkdir -p rooferpy
	python_module="$$(find ../install/lib -path '*/site-packages/roofer*.so' -type f -print -quit 2>/dev/null)"; \
	if [ -z "$$python_module" ]; then \
	  python_module="$$(find ../build/rooferpy -maxdepth 1 -type f -name 'roofer*.so' -print -quit 2>/dev/null)"; \
	fi; \
	if [ -z "$$python_module" ]; then \
	  echo "Could not find roofer Python extension in ../install/lib/*/site-packages or ../build/rooferpy"; \
	  exit 1; \
	fi; \
	echo "Using roofer Python extension: $$python_module"; \
	cp "$$python_module" rooferpy/
	$(PYTHON) -c "import sys; sys.path.insert(0, 'rooferpy'); import roofer"
	doc_helper="../install/bin/doc-helper"; \
	if [ ! -x "$$doc_helper" ]; then \
	  doc_helper="../build/apps/roofer-app/doc-helper"; \
	fi; \
	if [ ! -x "$$doc_helper" ]; then \
	  echo "Could not find executable doc-helper in ../install/bin or ../build/apps/roofer-app"; \
	  exit 1; \
	fi; \
	"$$doc_helper" config > example_full.toml; \
	"$$doc_helper" params > cli-options.md; \
	"$$doc_helper" attr > output-attributes.md
	doxygen Doxyfile
	@$(SPHINXBUILD) -M $@ "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)
