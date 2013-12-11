###################  sendLinkMail -- bobh@mbari.org  #######################
#    Copyright (C) 2006 MBARI
#    MBARI Proprietary Information. All rights reserved.
#
#   Send mail to users specified in /tmp/<interface>.mail when link
#   comes up
#
########################################################################

require 'net/smtp'

$iface=ARGV[0]
$filename = "/tmp/#{$iface}.mail"

SMTPServer  = "mail.shore.mbari.org"
SMTPport    = 25
HELOdomain  = "mbari.org"
MailFrom    = "esp@mbari.org"
MsgDelim    = "\n\n"

if FileTest.exist? $filename then

  smtp = Net::SMTP.start(SMTPServer, SMTPport, HELOdomain)

  fullmsg = "Subject: ESP link is up\nFrom: " + MailFrom + "\nTo: "
  tolist = []
  
  file = File.new($filename, "r")
  file.each {|line| 
    line.chomp!
    fullmsg << line+","
    tolist << line
  }

  fullmsg.chomp!(",")
  fullmsg << MsgDelim + "ESP link " + $iface + " is up\n"

  smtp.send_mail(fullmsg, MailFrom, tolist)
  smtp.finish

  file.close
  File.delete($filename)

end
