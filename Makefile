integration-test:
	./bootstrap
	./build/integration_tests

# Mainly used for manually exploring sourcekit things
experiment:
	./bootstrap
	./build/test_driver

# Consider expressing this better.
format:
	@$(shell for f in $$(ls *.{hpp,cpp}); do clang-format $$f; done)
