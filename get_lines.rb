#!/usr/bin/env ruby

require 'rugged'
require 'linguist'

path = File.dirname(__FILE__)
rugged = Rugged::Repository.new(path)
repo = Linguist::Repository.new(rugged, rugged.head.target_id)

file_breakdown = repo.breakdown_by_file
puts "%7s %s" % ["Lines", "Language"]
puts "------- --------"
file_breakdown.each do |lang, files|
  lines = 0
  files.each do |file|
    count = File.foreach(file).inject(0) {|c, line| c+1}
    lines += count
  end
  puts "%7s %s" % ["#{lines}", lang]
end

