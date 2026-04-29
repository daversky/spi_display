#!/bin/bash

# Проверка аргумента
if [ -z "$1" ]; then
    echo "Использование: $0 <полный_путь_к_файлу_ttf>"
    echo "Пример: $0 /usr/share/fonts/truetype/ubuntu/Ubuntu-R.ttf"
    exit 1
fi

FULL_PATH=$1
INPATH=$(dirname "$FULL_PATH")
FILE_WITH_EXT=$(basename "$FULL_PATH")
FONT_NAME="${FILE_WITH_EXT%.*}"
OUTPATH="./generated"
SIZES=(6 8 12 16 20 24)
mkdir -p "${OUTPATH}"

echo "--- Обработка шрифта ---"
echo "Файл: ${FILE_WITH_EXT}"
echo "Путь: ${INPATH}"
echo "Имя для кода: ${FONT_NAME}"
echo "------------------------"
ALL_H="${OUTPATH}/${FONT_NAME}_all.h"
echo "" > "${ALL_H}"
for SIZE in "${SIZES[@]}"
do
    LATIN_H="${OUTPATH}/${FONT_NAME}_${SIZE}_latin.h"
    CYRILLIC_H="${OUTPATH}/${FONT_NAME}_${SIZE}_cyrillic.h"
    echo "// ------------------------ ${FONT_NAME} ${SIZE} ------------------------" >> "${ALL_H}"
    ./fontconvert "$FULL_PATH" $SIZE 32 127 > "$LATIN_H" || exit 1
    ./fontconvert "$FULL_PATH" $SIZE 1025 1105 > "$CYRILLIC_H" || exit 1
    for FILE in "$LATIN_H" "$CYRILLIC_H"
    do
        sed -i '/#include/d' "$FILE"
        sed -i 's/PROGMEM //g' "$FILE"
    done
    SEARCH_NAME=$(echo "$FONT_NAME" | tr '-' '_')
    # Латиница
    # Заменяем только имя массива, не трогая (uint8_t *)
    sed -i "s/${SEARCH_NAME}[^,)]*Bitmaps/Font_L_${SIZE}_Bitmaps/g" "$LATIN_H"
    sed -i "s/${SEARCH_NAME}[^,)]*Glyphs/Font_L_${SIZE}_Glyphs/g" "$LATIN_H"
    sed -i "s/^const GFXfont.*/const GFXfont Font_L_${SIZE} = {/" "$LATIN_H"
    sed -i '/^$/d' "$LATIN_H"
    # Кириллица
    sed -i "s/${SEARCH_NAME}[^,)]*Bitmaps/Font_C_${SIZE}_Bitmaps/g" "$CYRILLIC_H"
    sed -i "s/${SEARCH_NAME}[^,)]*Glyphs/Font_C_${SIZE}_Glyphs/g" "$CYRILLIC_H"
    sed -i "s/^const GFXfont.*/const GFXfont Font_C_${SIZE} = {/" "$CYRILLIC_H"
    sed -i '/^$/d' "$CYRILLIC_H"

    cat "${LATIN_H}" >> "${ALL_H}"
    echo "" >> "${ALL_H}"
    cat "${CYRILLIC_H}" >> "${ALL_H}"
    echo "" >> "${ALL_H}"

done

echo "--------------------------------------"
echo "Готово! Результаты в папке: $OUTPATH"