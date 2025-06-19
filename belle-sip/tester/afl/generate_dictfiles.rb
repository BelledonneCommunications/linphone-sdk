# this script just reads each line in the sip_dict.txt and creates a file with the line content in it.
# this is foe AFL to get an idea of important lexemes to use to mutate SIP messages.

lines = File.open("sip_dict.txt", "r").read

# remove empty lines (this is bound to happen)
lines = lines.split(/\n/).reject{ |l| l.chomp.empty? }.join("\n")

lines.each_line { |line|  
    line.gsub!(/\n/, "")
    file = "sip_dict/#{line}"
    file.gsub!(/[\=\:]/,"_")
    print "Create file #{file}\n"
    # comment this line for a dry run
    #File.open(file, "w") { |file| file.write(line) }
}