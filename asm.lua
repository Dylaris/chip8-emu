local START_ADDR = 0x200

local function ignore_comment(line)
    return line:gsub("%s*;.*", ""):gsub("%s+$", "")
end

-- 'byte "hello", 0'
local function match_bytes(line)
    local byte_stream = {}
    line = ignore_comment(line)
    line = line:gsub("^%s*.byte%s*", "") or line:gsub("^%s*.BYTE%s*", "")

    if line ~= "" then
        local str = line:match('"([^"]+)"')
        if str then
            for i = 1, #str do
                table.insert(byte_stream, string.byte(str, i))
            end
            line = line:gsub('"[^"]+"', '', 1)
        end

        for byte in line:gmatch('([^,]+)') do
            byte = byte:match('^%s*(.-)%s*$')
            if byte then table.insert(byte_stream, tonumber(byte)) end
        end

    end

    -- for _, v in ipairs(byte_stream) do print(v) end
    return byte_stream
end

local consts = {}

-- define foo 12
local function match_const(line)
    line = ignore_comment(line)
    line = line:gsub("^%s*.define%s*", "") or line:gsub("^%s*.DEFINE%s*", "")
    if line ~= "" then
        local name = line:match("^%s*([%w%.%-%_]+)")
        line = line:gsub("^%s*(%w+)%s*", "")
        local val = tonumber(line:match("^%s*(%w+)"))

        local const = {name = name, value = val}
        table.insert(consts, const)
    end
end

-- parse the labels and constants
local labels = {}
local function parse_label_and_const(file)
    local offset = 0;
    for line in file:lines() do
        line = ignore_comment(line)
        if line ~= "" then
            local label = line:match("^%s*([%w%.%-%_%@]+):")
            local pseudo_code = line:match("^%s*([%w%.%-%_]+)")
            if label then
                labels[label] = offset
            elseif pseudo_code == ".byte" or pseudo_code == ".BYTE" then
                local byte_stream = match_bytes(line)
                offset = offset + ((#byte_stream == 0) and 2 or #byte_stream)
            elseif pseudo_code == ".define" or pseudo_code == ".DEFINE" then
                match_const(line)
            else
                offset = offset + 2
            end

        end
    end
end


--[[ 
Chip8 instruction format (2 bytes long):
1. opcode                               --> 00
2. opcode oprand1                       --> 01
3. opcode oprand1, oprand1              --> 10
4. opcode oprand1, oprand2, oprand3     --> 11
--]]

local function replace(oprand)
    for _, const in ipairs(consts) do
        if const.name == oprand then
            return const.value
        end
    end
    return oprand
end

local function equal(a, b)
    return (a == string.lower(b) or a == string.upper(b))
end

local function is_inst(line)
    local o = line:match("^%s*(%w+)")
    if line:match("^%s*(%w+):") or equal(o, ".byte") or equal(o, ".define") then
        return false
    end
    return true
end

local inst_set = {}    -- instruction
local function parse_inst(file)
    for line in file:lines() do
        line = ignore_comment(line)

        if line and is_inst(line) then
            local opcode = line:match("^%s*(%w+)")
            if opcode then
                line = line:gsub("^%s*(%w+)%s*", "")

                local oprands = {}
                for oprand in line:gmatch('([^,]+)') do
                    oprand = oprand:match('^%s*(.-)%s*$')
                    if oprand then table.insert(oprands, replace(oprand)) end
                end

                local inst = {opcode = opcode, oprands = oprands}
                table.insert(inst_set, inst)
            end
        end
    end
end

-- generate machine code
local codes = {}
local function to_big_endian(code)
    local msb = (code & 0xFF00) >> 8
    local lsb = code & 0x00FF

    table.insert(codes, msb)
    table.insert(codes, lsb)
end

local function to_address(oprand)
    local addr = tonumber(oprand)
    if not addr then    -- label
        if not labels[oprand] then  -- invalid label
            print("invalid label: " .. oprand)
            os.exit(1)
        end
        addr = START_ADDR + labels[oprand]
    end
    return addr
end

local function get_reg_id(reg)
    local id = string.format("0x%s", string.sub(reg, -1))
    return tonumber(id)
end


local function gen_code_fmt00(inst)
    if inst.opcode == "cls" or inst.opcode == "CLS" then
        to_big_endian(0x00E0)
    elseif inst.opcode == "ret" or inst.opcode == "RET" then
        to_big_endian(0x00EE)
    end
end

local function gen_code_fmt01(inst)
    if equal(inst.opcode, "sys") then
        to_big_endian((0 << 12) | to_address(inst.oprands[1]))
    elseif equal(inst.opcode, "jp") then
        to_big_endian((1 << 12) | to_address(inst.oprands[1]))
    elseif equal(inst.opcode, "call") then
        to_big_endian((2 << 12) | to_address(inst.oprands[1]))
    elseif equal(inst.opcode, "skp") then
        to_big_endian((0xE << 12) | (get_reg_id(inst.oprands[1]) << 8) | 0x9E)
    elseif equal(inst.opcode, "sknp") then
        to_big_endian((0xE << 12) | (get_reg_id(inst.oprands[1]) << 8) | 0xA1)
    end
end

local function gen_code_fmt10(inst)
    if equal(inst.opcode, "se") then
        if tonumber(inst.oprands[2]) then
            to_big_endian((3 << 12) | (get_reg_id(inst.oprands[1]) << 8)
                          | tonumber(inst.oprands[2]))
        else
            to_big_endian((5 << 12) | (get_reg_id(inst.oprands[1]) << 8)
                          | (get_reg_id(inst.oprands[2]) << 4) | 0x00)
        end
    elseif equal(inst.opcode, "sne") then
        if tonumber(inst.oprands[2]) then
            to_big_endian((4 << 12) | (get_reg_id(inst.oprands[1]) << 8)
                          | tonumber(inst.oprands[2]))
        else
            to_big_endian((9 << 12) | (get_reg_id(inst.oprands[1]) << 8)
                          | (get_reg_id(inst.oprands[2]) << 4) | 0x00)
        end
    elseif equal(inst.opcode, "ld") then
        if equal(inst.oprands[1], "[i]") then
            to_big_endian((0xF << 12) | (get_reg_id(inst.oprands[2]) << 8) | 0x55)
        elseif equal(inst.oprands[1], "i") then
            to_big_endian((0xA << 12) | tonumber(inst.oprands[2]))
        elseif equal(inst.oprands[1], "b") then
            to_big_endian((0xF << 12) | (get_reg_id(inst.oprands[2]) << 8) | 0x33)
        elseif equal(inst.oprands[1], "f") then
            to_big_endian((0xF << 12) | (get_reg_id(inst.oprands[2]) << 8) | 0x29)
        elseif equal(inst.oprands[1], "dt") then
            to_big_endian((0xF << 12) | (get_reg_id(inst.oprands[2]) << 8) | 0x15)
        elseif equal(inst.oprands[1], "st") then
            to_big_endian((0xF << 12) | (get_reg_id(inst.oprands[2]) << 8) | 0x18)
        else
            if equal(inst.oprands[2], "[i]") then
                to_big_endian((0xF << 12) | (get_reg_id(inst.oprands[1]) << 8) | 0x65)
            elseif equal(inst.oprands[2], "k") then
                to_big_endian((0xF << 12) | (get_reg_id(inst.oprands[1]) << 8) | 0x0A)
            elseif equal(inst.oprands[2], "dt") then
                to_big_endian((0xF << 12) | (get_reg_id(inst.oprands[1]) << 8) | 0x07)
            elseif tonumber(inst.oprands[2]) then
                to_big_endian((6 << 12) | (get_reg_id(inst.oprands[1]) << 8)
                                        | tonumber(inst.oprands[2]))
            else
                to_big_endian((8 << 12) | (get_reg_id(inst.oprands[1]) << 8)
                            | (get_reg_id(inst.oprands[2]) << 4) | 0x00)
            end
        end
    elseif equal(inst.opcode, "add") then
        if equal(inst.oprands[1], "i") then
            to_big_endian((0xF << 12) | (get_reg_id(inst.oprands[2]) << 8) | 0x1E)
        else
            if tonumber(inst.oprands[2]) then
                to_big_endian((7 << 12) | (get_reg_id(inst.oprands[1]) << 8)
                            | tonumber(inst.oprands[2]))
            else
                to_big_endian((8 << 12) | (get_reg_id(inst.oprands[1]) << 8)
                            | (get_reg_id(inst.oprands[2]) << 4) | 0x04)
            end
        end
    elseif equal(inst.opcode, "sub") then
        to_big_endian((8 << 12) | (get_reg_id(inst.oprands[1]) << 8)
                      | (get_reg_id(inst.oprands[2]) << 4) | 0x05)
    elseif equal(inst.opcode, "subn") then
        to_big_endian((8 << 12) | (get_reg_id(inst.oprands[1]) << 8)
                      | (get_reg_id(inst.oprands[2]) << 4) | 0x07)
    elseif equal(inst.opcode, "or") then
        to_big_endian((8 << 12) | (get_reg_id(inst.oprands[1]) << 8)
                      | (get_reg_id(inst.oprands[2]) << 4) | 0x01)
    elseif equal(inst.opcode, "and") then
        to_big_endian((8 << 12) | (get_reg_id(inst.oprands[1]) << 8)
                      | (get_reg_id(inst.oprands[2]) << 4) | 0x02)
    elseif equal(inst.opcode, "xor") then
        to_big_endian((8 << 12) | (get_reg_id(inst.oprands[1]) << 8)
                      | (get_reg_id(inst.oprands[2]) << 4) | 0x03)
    elseif equal(inst.opcode, "shr") then
        to_big_endian((8 << 12) | (get_reg_id(inst.oprands[1]) << 8)
                      | (get_reg_id(inst.oprands[2]) << 4) | 0x06)
    elseif equal(inst.opcode, "shl") then
        to_big_endian((8 << 12) | (get_reg_id(inst.oprands[1]) << 8)
                      | (get_reg_id(inst.oprands[2]) << 4) | 0x0E)
    elseif equal(inst.opcode, "jp") then
        to_big_endian((0xB << 12) | to_address(inst.oprands[2]))
    elseif equal(inst.opcode, "rnd") then
        to_big_endian((0xC << 12) | (get_reg_id(inst.oprands[1]) << 8)
                                  | inst.oprands[2])
    end
end

local function gen_code_fmt11(inst)
    if equal(inst.opcode, "drw") then
        to_big_endian((0xD << 12) | (get_reg_id(inst.oprands[1]) << 8) |
                      (get_reg_id(inst.oprands[2]) << 4) | tonumber(inst.oprands[3]))
    end
end

local function gen_code_fmt()
    for _, inst in ipairs(inst_set) do
        local fmt = #inst.oprands
        if fmt == 0 then
            gen_code_fmt00(inst)
        elseif fmt == 1 then
            gen_code_fmt01(inst)
        elseif fmt == 2 then
            gen_code_fmt10(inst)
        else
            gen_code_fmt11(inst)
        end
    end
end

local function assembler()
    -- read input file
    if not arg[1] then
        print("Input file is necessary!")
        os.exit(1)
    end
    local file = io.open(arg[1], "r")
    if not file then
        print("Faild to open file: " .. arg[1])
        os.exit(1)
    end

    parse_label_and_const(file)
    file:seek("set", 0)
    parse_inst(file)
    gen_code_fmt()

    file:close()
    file = io.open("out", "wb")
    if not file then
        print("Faild to crete output file!")
        os.exit(1)
    end
    for _, code in ipairs(codes) do
        file:write(string.char(code))
    end
    file:close()
end

local function print_sort_lables()
    local sorted_labels = {}

    for label, offset in pairs(labels) do
        table.insert(sorted_labels, {label, offset})
    end

    table.sort(sorted_labels, function (a, b)
        return a[2] < b[2]
    end)

    for idx, pair in ipairs(sorted_labels) do
        print(idx, pair[1], pair[2])
    end
end

local function debug()
    assembler()
    print("----------label----------")
    print_sort_lables()

    print("----------constant----------")
    for idx, const in ipairs(consts) do
        print(idx, const.name, const.value)
    end
end

-- debug()
assembler()
