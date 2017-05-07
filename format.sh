# Use the following formatters
# clang-format

find . -name "*.cpp" -maxdepth 1 | while read line; do
    clang-format -i "$line"
done

find . -name "*.h" -maxdepth 1 | while read line; do
    clang-format -i "$line"
done

find . -name "*.hpp" -maxdepth 1 | while read line; do
    clang-format -i "$line"
done

