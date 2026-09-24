#include <Arduino.h>
#include <Keypad.h>

namespace 
{
    constexpr byte keypadRows = 1;
    constexpr byte keypadColumns = 5;
    char keypadKeys[keypadRows][keypadColumns] = {{'N', 'B', 'I', 'D', 'S'}};
    byte keypadRowPins[keypadRows] = {35};
    byte keypadColumnPins[keypadColumns] = {30, 31, 32, 33, 34};
    Keypad keypad = Keypad(makeKeymap(keypadKeys), keypadRowPins, keypadColumnPins,
                        keypadRows, keypadColumns);

}

char readKeypadKey()
{
    return keypad.getKey();
}