#!/bin/sh

dir=`dirname "$0"`
cd "$dir"

if [ ! -d mpris-remote ]
then
    git clone git@github.com:mackstann/mpris-remote.git
fi

remote="mpris-remote/mpris-remote"

call()
{
    dbus-send --session --type=method_call --print-reply --dest=org.mpris.corn $1 $2 $3 $4 $5 $6 $7 $8 $9
}

file1="`pwd`/Chamberlain-war-declaration.ogg"
file2="`pwd`/Home_on_the_range.ogg"

################################################

"$remote" clear

"$remote" addtrack "$file1"
"$remote" addtrack "$file2"

call /TrackList org.freedesktop.MediaPlayer.DelTrack int32:-1     >/dev/null
call /TrackList org.freedesktop.MediaPlayer.DelTrack int32:-100   >/dev/null
call /TrackList org.freedesktop.MediaPlayer.DelTrack int32:3      >/dev/null
call /TrackList org.freedesktop.MediaPlayer.DelTrack int32:100000 >/dev/null

#"$call" /Corn org.corn.CornPlayer.Move int32:

if [ `"$remote" numtracks` != 2 ]
then
    echo XXXXXXXXXXXXXXXXXX move tracks broken XXXXXXXXXXXXXXXXXXX
else
    echo move tracks ok
fi

################################################

"$remote" random true
"$remote" loop true
"$remote" repeat false
"$remote" clear

"$remote" addtrack "$file1"
"$remote" addtrack "$file2"

"$remote" play

song1plays_at_rand_pos0=0
song2plays_at_rand_pos0=0
for i in `seq 20`
do
    song1plays_at_rand_pos0=$(($song1plays_at_rand_pos0 + `"$remote" trackinfo | grep ^mrl: | grep -c Chamberlain`))
    song2plays_at_rand_pos0=$(($song2plays_at_rand_pos0 + `"$remote" trackinfo | grep ^mrl: | grep -c Home`))
    "$remote" next
    "$remote" next
    printf .
done

if [ "$song1plays_at_rand_pos0" = 0 -o "$song2plays_at_rand_pos0" = 0 ]
then
    echo XXXXXXXXXXXXXXXXXX shuffle sucks XXXXXXXXXXXXXXXXXXX
else
    echo shuffle good
fi

