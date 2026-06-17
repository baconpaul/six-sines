convert ../icon/SixSinesIcon.png --geometry 512x512 tmp.png
sips -i tmp.png -o SixSinesIcon.icns
rm tmp.png
DeRez -only icns SixSinesIcon.icns > icns.rsrc
