/******************************************************************************
  Target Script for Raspberry Pi RP2040

  THIS FILE IS PROVIDED AS IS WITH NO WARRANTY OF ANY KIND, INCLUDING THE
  WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 ******************************************************************************/

function Reset()
{
  var impl = TargetInterface.implementation ? TargetInterface.implementation() : "";
  if (impl == "j-link")
    TargetInterface.resetAndStop(100);
  else
    {
//      TargetInterface.pokeWord(0xE000EDFC, 0x00000001);
//      TargetInterface.pokeWord(0xE000ED0C, 0x05FA0004);
      TargetInterface.pokeWord(0xE000EDF0, 0xA05F0001);
      TargetInterface.delay(100);
      TargetInterface.resetDebugInterface();
      TargetInterface.pokeWord(0xE000EDF0, 0xA05F0003);
      TargetInterface.waitForDebugState(100);
    }
}

function GetPartName()
{
  var result = "";
  var corename = "";

  var core = TargetInterface.peekWord(0xD0000000);
  switch (core)
  {
  case 0:
    corename = "Core0";
    break;
  case 1:
    corename = "Core1";
    break;
  }

  var magic = TargetInterface.peekWord(0x10);
  switch (magic)
  {
  case 0x0101754D:
    result = "RP2040 B0 " + corename;
    break;
  case 0x0201754D:
    result = "RP2040 B1 " + corename;
    break;
  }

  return result;
}