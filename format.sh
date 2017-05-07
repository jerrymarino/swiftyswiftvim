# Use the following formatters
# clang-format
# which autopep8

find . -name "*.cpp" -maxdepth 1 | while read line; do
    clang-format -i "$line"
done

find . -name "*.h" -maxdepth 1 | while read line; do
    clang-format -i "$line"
done

find . -name "*.hpp" -maxdepth 1 | while read line; do
    clang-format -i "$line"
done

find . -name "*.py" -maxdepth 1 | while read line; do
    autopep8 --in-place --aggressive --aggressive "$line"
done

