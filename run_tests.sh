reset;
for f in `find testdata -maxdepth 1 -type f`; do \
    echo;
    ./build/dp4c $f;
    if [ $? -eq 0 ]; then
        echo "$f : PASSED"
    else
        echo "$f : FAILED"
    fi
done
