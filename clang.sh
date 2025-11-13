find ./src -regex '.*\.\(cpp\|hpp\|ipp\|cc\|cxx\)' -exec clang-format -i {}  \;
find ./tests -regex '.*\.\(cpp\|hpp\|ipp\|cc\|cxx\)' -exec clang-format -i {}  \;
