#!/usr/bin/ruby

ESCAPE_SEQUENCES = 
  /(\e[78]|\e\[(\?25[lh]|2[JK]|\d{1,2}m|\d+;\d+[rH]))+/

ARGV.each do |filename|
  cleaned_name = filename + "_clean"
  File.open(filename, 'r') do |file|
    File.open(cleaned_name, 'w') do |clean|
      while(line = file.gets)
        clean.puts(line.chomp.gsub(ESCAPE_SEQUENCES, "\n"))
      end
    end
  end
end
    
