#!/bin/bash

BUILD_ENV=$1

APP_FILES=$(find ./src/app -name "*.c")
SRC_FILES="$APP_FILES ./src/linux.c ./src/db.c"

if [ "$BUILD_ENV" == "dev" ]; then
    CFLAGS="-std=c89 -g -DDEBUG=1 -Wall -Wextra -Werror -pedantic -Wno-declaration-after-statement -Wno-unused-variable -Wno-unused-parameter -Wno-long-long" 

    gcc $CFLAGS \
        $SRC_FILES \
        -o "littlechef-dev" \
        -I./src \
        -I../sqlite-amalgamation-3480000 \
        -L../sqlite-amalgamation-3480000 \
        -lsqlite3 \
        -largon2 \
        -pthread \
        -lcrypto
elif [ "$BUILD_ENV" == "prod" ]; then
    BUILD_DIR_NAME="build"

    rm -rf "./$BUILD_DIR_NAME"
    mkdir -p "./$BUILD_DIR_NAME"
    
    CFLAGS="-std=c89 -O3 -DDEBUG=0"

    gcc $CFLAGS \
        $SRC_FILES \
        -o "./$BUILD_DIR_NAME/littlechef" \
        -lsqlite3 \
        -largon2 \
        -pthread \
        -lcrypto

    ASSETS_SOURCE_DIR="$(pwd)/assets/"
    DEST_DIR="$(pwd)/$BUILD_DIR_NAME/"

    cp -r "$ASSETS_SOURCE_DIR" "$DEST_DIR"
    cp ./littlechef-dev.db "$DEST_DIR"
fi

echo "Done!"