// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only initializer bridge for the SDL/hostgen path. The retail
// initializer table calls ResetPacketWriters explicitly; the host CRT does
// not know about that EE callback. Keep this separate from decomp source.
void func_0015F658(void);

void func_001FAD68(void)
{
    func_0015F658();
}
