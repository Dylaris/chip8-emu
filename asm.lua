local START_ADDR = 0x200

local function ignore_comment(line)
    return line:gsub("%s*;.*", ""):gsub("%s+$", "")
end

-- '.byte "hello", 0'
local function match_bytes(line)
    local byte_stream = {}
    line = line:gsub("^%s*%.byte ", "")
    line = ignore_comment(line)

    if line then
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

-- parse the labels
local labels = {}

local function parse_label(file)
    local offset = 0;
    for line in file:lines() do
        line = ignore_comment(line)
        if line ~= "" then
            local label = line:match("^%s*([%w%.%-%_]+):")
            if label then
                labels[label] = offset
            else
                local byte_stream = match_bytes(line)
                offset = offset + ((#byte_stream == 0) and 2 or #byte_stream)
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

local inst_set = {}    -- instruction
local function parse_inst(file)
    for line in file:lines() do
        line = ignore_comment(line)

        if line and not line:match("^%s*(%w+):") then
            local opcode = line:match("^%s*(%w+)")
            if opcode then
                line = line:gsub("^%s*(%w+)%s*", "")

                local oprands = {}
                for oprand in line:gmatch('([^,]+)') do
                    oprand = oprand:match('^%s*(.-)%s*$')
                    if oprand then table.insert(oprands, oprand) end
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

local function inst_equal(opcode, inst)
    return (opcode == string.lower(inst) or opcode == string.upper(inst))
end

local function gen_code_fmt00(inst)
    if inst.opcode == "cls" or inst.opcode == "CLS" then
        to_big_endian(0x00E0)
    elseif inst.opcode == "ret" or inst.opcode == "RET" then
        to_big_endian(0x00EE)
    end
end

local function gen_code_fmt01(inst)
    if inst_equal(inst.opcode, "sys") then
        to_big_endian((0 << 12) | to_address(inst.oprands[1]))
    elseif inst_equal(inst.opcode, "jp") then
        to_big_endian((1 << 12) | to_address(inst.oprands[1]))
    elseif inst_equal(inst.opcode, "call") then
        to_big_endian((2 << 12) | to_address(inst.oprands[1]))
    elseif inst_equal(inst.opcode, "skp") then
        to_big_endian((0xE << 12) | (get_reg_id(inst.oprands[1]) << 8) | 0x9E)
    elseif inst_equal(inst.opcode, "sknp") then
        to_big_endian((0xE << 12) | (get_reg_id(inst.oprands[1]) << 8) | 0xA1)
    end
end

local function gen_code_fmt10(inst)
    if inst_equal(inst.opcode, "se") then
        if tonumber(inst.oprands[2]) then
            to_big_endian((3 << 12) | (get_reg_id(inst.oprands[1]) << 8)
                          | tonumber(inst.oprands[2]))
        else
            to_big_endian((5 << 12) | (get_reg_id(inst.oprands[1]) << 8)
                          | (get_reg_id(inst.oprands[2]) << 4) | 0x00)
        end
    elseif inst_equal(inst.opcode, "sne") then
        if tonumber(inst.oprands[2]) then
            to_big_endian((4 << 12) | (get_reg_id(inst.oprands[1]) << 8)
                          | tonumber(inst.oprands[2]))
        else
            to_big_endian((9 << 12) | (get_reg_id(inst.oprands[1]) << 8)
                          | (get_reg_id(inst.oprands[2]) << 4) | 0x00)
        end
    elseif inst_equal(inst.opcode, "ld") then
        if inst_equal(inst.oprands[1], "[i]") then
            to_big_endian((0xF << 12) | (get_reg_id(inst.oprands[2]) << 8) | 0x55)
        elseif inst_equal(inst.oprands[1], "i") then
            to_big_endian((0xA << 12) | tonumber(inst.oprands[2]))
        elseif inst_equal(inst.oprands[1], "b") then
            to_big_endian((0xF << 12) | (get_reg_id(inst.oprands[2]) << 8) | 0x33)
        elseif inst_equal(inst.oprands[1], "f") then
            to_big_endian((0xF << 12) | (get_reg_id(inst.oprands[2]) << 8) | 0x29)
        elseif inst_equal(inst.oprands[1], "dt") then
            to_big_endian((0xF << 12) | (get_reg_id(inst.oprands[2]) << 8) | 0x15)
        elseif inst_equal(inst.oprands[1], "st") then
            to_big_endian((0xF << 12) | (get_reg_id(inst.oprands[2]) << 8) | 0x18)
        else
            if inst_equal(inst.oprands[2], "[i]") then
                to_big_endian((0xF << 12) | (get_reg_id(inst.oprands[1]) << 8) | 0x65)
            elseif inst_equal(inst.oprands[2], "k") then
                to_big_endian((0xF << 12) | (get_reg_id(inst.oprands[1]) << 8) | 0x0A)
            elseif inst_equal(inst.oprands[2], "dt") then
                to_big_endian((0xF << 12) | (get_reg_id(inst.oprands[1]) << 8) | 0x07)
            elseif tonumber(inst.oprands[2]) then
                to_big_endian((6 << 12) | (get_reg_id(inst.oprands[1]) << 8)
                                        | tonumber(inst.oprands[2]))
            else
                to_big_endian((8 << 12) | (get_reg_id(inst.oprands[1]) << 8)
                            | (get_reg_id(inst.oprands[2]) << 4) | 0x00)
            end
        end
    elseif inst_equal(inst.opcode, "add") then
        if inst_equal(inst.oprands[1], "i") then
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
    elseif inst_equal(inst.opcode, "sub") then
        to_big_endian((8 << 12) | (get_reg_id(inst.oprands[1]) << 8)
                      | (get_reg_id(inst.oprands[2]) << 4) | 0x05)
    elseif inst_equal(inst.opcode, "subn") then
        to_big_endian((8 << 12) | (get_reg_id(inst.oprands[1]) << 8)
                      | (get_reg_id(inst.oprands[2]) << 4) | 0x07)
    elseif inst_equal(inst.opcode, "or") then
        to_big_endian((8 << 12) | (get_reg_id(inst.oprands[1]) << 8)
                      | (get_reg_id(inst.oprands[2]) << 4) | 0x01)
    elseif inst_equal(inst.opcode, "and") then
        to_big_endian((8 << 12) | (get_reg_id(inst.oprands[1]) << 8)
                      | (get_reg_id(inst.oprands[2]) << 4) | 0x02)
    elseif inst_equal(inst.opcode, "xor") then
        to_big_endian((8 << 12) | (get_reg_id(inst.oprands[1]) << 8)
                      | (get_reg_id(inst.oprands[2]) << 4) | 0x03)
    elseif inst_equal(inst.opcode, "shr") then
        to_big_endian((8 << 12) | (get_reg_id(inst.oprands[1]) << 8)
                      | (get_reg_id(inst.oprands[2]) << 4) | 0x06)
    elseif inst_equal(inst.opcode, "shl") then
        to_big_endian((8 << 12) | (get_reg_id(inst.oprands[1]) << 8)
                      | (get_reg_id(inst.oprands[2]) << 4) | 0x0E)
    elseif inst_equal(inst.opcode, "jp") then
        to_big_endian((0xB << 12) | to_address(inst.oprands[2]))
    elseif inst_equal(inst.opcode, "rnd") then
        to_big_endian((0xC << 12) | (get_reg_id(inst.oprands[1]) << 8)
                                  | inst.oprands[2])
    end
end

local function gen_code_fmt11(inst)
    if inst_equal(inst.opcode, "drw") then
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

    parse_label(file)
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

    for _, pair in ipairs(sorted_labels) do
        print("", pair[1], pair[2])
    end
end

local function debug()
    assembler()
    print_sort_lables()
end

-- debug()
assembler()
