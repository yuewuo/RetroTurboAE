-- you can click buttons below to select a demo code
-- or click above to use library code

collection = "test190608"
id = "5cfa8ec635130000760054f9"

local record_id = set_record(1)
sleepms(100)
count, real_frequency = tag_send(collection, id, "frequency", "o_out")
logln(count)
logln(real_frequency)
set_record(0)
rt.reader.plot_rx_data(record_id)
