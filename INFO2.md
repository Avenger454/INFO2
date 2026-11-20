# INFO2 first screen

Fuel percentage and battery volts from the supplied layout. The INFO2 heading
is hidden, leaving blank future-header space with both red accents retained.
Live numeric widgets cover sample values. Two 7px bright red needles span the
blue arc bands with bounded redraws and unchanged-value/pixel skips.

Demo mode retains a 30-second up/down sweep: fuel 0..100%, battery 11..15 V.
With demo disabled, fuel uses the existing input; battery displays -- until
verified voltage data is connected using ui_info2_set_values(fuel_pct, volts).
NAN/out-of-range input displays -- and hides that needle. Values outside the
11..15 battery scale retain their numeric value while the needle clamps to
its nearest endpoint. No fabricated battery data source is added.

Display initialization, DMA buffers and DSI compatibility are retained.
ESP-IDF hardware build and real-data validation remain necessary.
