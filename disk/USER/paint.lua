print("Fast /dev/fb0 Paint! Click inside QEMU & draw (1=Red 2=Green 3=Gold 4=Purple, q=Quit)")
local mf = io.open("/dev/mouse", "rb")
local kf = io.open("/dev/kbd", "rb")
local fb = io.open("/dev/fb0", "wb")
mf:setvbuf("no"); kf:setvbuf("no"); fb:setvbuf("no")

-- Auto-detect resolution (800x600 in BIOS vs 1280x800 in UEFI)
local W, H = 1280, 800
local md0 = mf:read(12)
if md0 and #md0 == 12 then
    local ix, iy = string.unpack("<ii", md0)
    if ix == 400 and iy == 300 then W, H = 800, 600 end
end

local colors = { ["1"] = 0x00EF4444, ["2"] = 0x0010B981, ["3"] = 0x00FACC15, ["4"] = 0x00A855F7 }
local active_col = 0x00EF4444
local last_x, last_y, last_btn = -1, -1, -1

while true do
    local kd = kf:read(2)
    if kd and #kd >= 1 then
        local ch = kd:sub(1, 1)
        if ch == "q" then break end
        if colors[ch] then active_col = colors[ch] end
    end

    local md = mf:read(12)
    if md and #md == 12 then
        local x, y, btn = string.unpack("<iii", md)
        if x ~= last_x or y ~= last_y or btn ~= last_btn then
            last_x, last_y, last_btn = x, y, btn
            local col = (btn ~= 0) and active_col or 0x0038BDF8
            local brush = string.rep(string.pack("<I4", col), 12)
            for dy = 0, 11 do
                local py = y + dy
                if py >= 0 and py < H and x >= 0 and x + 12 <= W then
                    fb:seek("set", (py * W + x) * 4)
                    fb:write(brush)
                end
            end
        end
    end
end

mf:close(); kf:close(); fb:close()
print("Exited paint.lua!")
