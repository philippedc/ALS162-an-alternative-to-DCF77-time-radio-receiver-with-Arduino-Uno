# ALS162-an-alternative-to-DCF77-time-radio-receiver-with-Arduino-Uno

Before the world of the internet and GPS, the way to set the same time throughout a country was to use a radio station. There were at least 5 of these special radio stations in Europe until the 1990s, of which only 2 are still operating today: the very popular DCF77 in Germany and the rather unknown ALS162 in France.

DCF77 is a 50 kW, 77.5 kHz transmitter based at Mainflingen in Germany, detail can be found here: https://en.wikipedia.org/wiki/DCF77 

ALS162 is a 800 kW, 162 kHz transmitter based at Allouis in France, detail are available here: 
in English https://en.wikipedia.org/wiki/ALS162_time_signal
or in French https://syrte.obspm.fr/spip/services/ref-temps/article/mise-a-disposition-du-temps-legal-par-le-signal-als162

Both use similar time signal code sequence.

Most descriptions of time radio devices made or sold are based on DCF77. To make things easier for the do-it-yourselfer, there are also ready-made receiver modules for DCF77, made in China and very, very cheap... 

Also, ALS162 is much less documented than DCF77, and can be considered more difficult to build, as it is a phase-modulated signal, while DCF77 is a very simple AM signal. 
However, if you want to build your own receiver instead of using these ready-made modules, you will understand how hard it is to get the little LED to blink every second. Then it looks much easier to build a 162 kHz direct amplified receiver, as the power of the radio transmitter gives the ALS162 the advantage.

As DCF77 is modulated in AM, the first thought is that it is easy to handle, but it is also very sensitive to electromagnetic noise. So sensitive, in fact, that it is almost necessary to move the microprocessor used to decode and display the time away from the receiver, at the risk of interfering with the reception of the signal.
Instead, the ALS162, with its phase modulation method (very similar to FM), gains a very great advantage in terms of immunity to electromagnetic noise. Even all those modern LED lamps with their buck converters, as well as any device that requires a voltage converter power supply, are no longer a threat.

This manual explains step by step how to build an ALS162 receiver with its time decoder in the simplest way possible.

