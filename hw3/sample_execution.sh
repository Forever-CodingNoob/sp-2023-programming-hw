#!/bin/sh

sample_1() {
    ./main 3 6 10 0 0
}

sample_2() {
    ./main 3 5 3 0 0 &
    child=$!
    sleep 1.5
    kill -TSTP $child
    sleep 2
    kill -TSTP $child
    sleep 3
    kill -TSTP $child
    wait $child
}

sample_3() {
    ./main 3 0 0 200 -900 &
    child=$!
    sleep 0.5
    kill -TSTP $child
    wait $child
}

print_help() {
    echo "usage: $0 [subtask]"
}

main() {
    case "$1" in
        1)
            sample_1
            ;;
        2)
            sample_2
            ;;
        3)
            sample_3
            ;;
        *)
            print_help
            ;;
    esac
}

main "$1"
