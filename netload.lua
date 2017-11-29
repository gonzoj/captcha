local captcha = require("captcha")

local knn = captcha.solver("knn", "netload")

local function filter_regions_of_interest(r)
	local f = {}
	for _, a in ipairs(r) do
		if #f == 4 then
			table.sort(f, function(x, y) return x.size < y.size end)
			if a.size > f[1].size then
				table.remove(f, 1)
				table.insert(f, a)
			end
		elseif #f > 0 then
			local add = true
			for i, b in ipairs(f) do
				if a:intersects(b) then
					if a.size > b.size then
						table.remove(f, i)
					else
						add = false
					end
					break
				elseif math.abs(a.width - b.width) < 4 and math.abs(a.x - b.x) < 4 then
					a:concat(b)
					table.remove(f, i)
					break
				end
			end
			if add then
				table.insert(f, a)
			end
		else
			table.insert(f, a)
		end
	end
	table.sort(f, function(x, y) return x.x < y.x end)
	return f
end

local function extract_feature_vector(r)
	local f = {}
	r:resize(12, 18)
	for i = 1, r.size do
		table.insert(f, r.data[i])
	end
	return f
end

local function solve(b)
	local img, msg = captcha.load(b)
	if not img then
		return nil, msg
	end
	img:grayscale()
	img:blur(3, 2)
	img:threshold(41, 24)
	local roi = filter_regions_of_interest(img:regions())
	if #roi < 4 then
		return nil, "failed to extract characters"
	end
	local samples = {}
	for _, r in ipairs(roi) do
		table.insert(samples, extract_feature_vector(img:crop(r)))
	end
	for _, r in ipairs(roi) do
		img:rectangle(r, 255, 0, 0)
	end
	img:show("captcha")
	img:input()
	img:hide()
	return knn:solve(samples)
end

local solutions = captcha.database("test").response:load()

local file = io.open("test/" .. arg[1], "rb")
local buf = file:read("*a")
file:close()

local s, e = solve(buf)
if s then
	io.write("SOLUTION: " .. s)
	if s ~= solutions[arg[1]] then
		io.write(" - WRONG (" .. solutions[arg[1]] .. ")")
	end
	print()
else
	print("ERROR: " .. e)
end
