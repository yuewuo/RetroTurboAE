-- test python script to evaluate multiple tasks simultaneously

-- evaluate_idx = 0  -- only for debugging, input using python script instead
-- evaluate_cnt = 10  -- only for debugging, input using python script instead

i = 0
show_at = 10000000
cnt = 0

while cnt < evaluate_cnt do
    i = i + 1
    if i % show_at == 0 then
        cnt = cnt + 1
        logln("evaluate_idx("..evaluate_idx..") cnt("..cnt..")")
        i = 0
    end
end
