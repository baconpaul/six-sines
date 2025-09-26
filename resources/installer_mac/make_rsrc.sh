convert ../SideQuestIcon.png --geometry 512x512 tmp.png
sips -i tmp.png -o SideQuestIcon.icns
rm tmp.png
DeRez -only icns SideQuestIcon.icns > icns.rsrc
