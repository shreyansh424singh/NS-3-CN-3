set terminal png
set output "congestion.png"
set title "Congestion Window Calculation"
set xlabel "Time (in Seconds)"
set ylabel "Congestion Window (cwnd)"
plot "Task.cwnd" using 1:3 with linespoint title "Congestion Window"
