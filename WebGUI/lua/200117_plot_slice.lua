-- To demonstrate how to plot a slice of data inside a huge record

function time2idx(ms)
    return math.floor(ms * 56.875 + 0.5)
end

start_ms = 10
duration_ms = 10  -- plotting 10ms ~ 20ms

plot("5E1F056E50410000EA006994", time2idx(start_ms), time2idx(duration_ms))
