package CCD;
#use     strict;
use     PDL;
use     Exporter;
use     vars qw(@ISA @EXPORT @EXPORT_OK @EXPORT_TAGS $VERSION);
@ISA    = qw(Exporter);
@EXPORT = qw(
    new
    expose
);
$VERSION = 0.01;

sub new {
    my $ccd   = {};
    my $self  = shift;
    my $class = ref($self) || $self;
    my $dev   = shift;
    my $query = '';
    $dev = '/dev/ccdA' if $dev eq '';
    
    open CCD_DEVICE, "+<$dev" or die "Can't open CCD device $dev";
    ${$ccd}{device} = $dev;
    
    print CCD_DEVICE "<QUERY/>";
    if (($query = <CCD_DEVICE>) =~ /</i) {
    	$query            =~ /CCDNAME\s*=\s*\"(.+)\"/i;
    	${$ccd}{name}    =  $1;
    	$query            =~ /HEIGHT\s*=\s*(\d+)/i;
    	${$ccd}{height}  =  $1;
    	$query            =~ /WIDTH\s*=\s*(\d+)/i;
    	${$ccd}{width}   =  $1;
        ${$ccd}{xbin}    = 1;
        ${$ccd}{ybin}    = 1;
        ${$ccd}{xoffset} = 0;
        ${$ccd}{yoffset} = 0;
        ${$ccd}{xsub}    = ${$ccd}{width};
        ${$ccd}{ysub}    = ${$ccd}{height};
        bless $ccd, $class;
    } else {
        unref $ccd;
    }
    close CCD_DEVICE;
    return $ccd;
}

#
# Expose an image frame for msecs.
#
sub expose {
    my ($self, $msec) = @_;
    my $header, $pixel_array, $pixel, $i, $j, $width, $height;

    if (ref($self)) {
        open CCD_DEVICE, "+<${$self}{device}" or die "Can't open CCD device ${$self}{device}";
        print CCD_DEVICE "<EXPOSE XOFFSET=${$self}{xoffset} YOFFSET=${$self}{yoffset} XBIN=${$self}{xbin} YBIN=${$self}{ybin} WIDTH=${$self}{xsub} HEIGHT=${$self}{ysub} MSEC=$msec/>";
    } else {
        $msec = $self if ($self ne "CCD");
        open CCD_DEVICE, "+</dev/ccdA" or die "Can't open CCD device /dev/ccdA";
        print CCD_DEVICE "<EXPOSE MSEC=$msec/>";
    }
    
    if (($header = <CCD_DEVICE>) =~ /<IMAGE/i) {
        $header =~ /HEIGHT\s*=\s*(\d+)/i;
        $height =  $1;
        $header =~ /WIDTH\s*=\s*(\d+)/i;
        $width  =  $1;
        $pixel_array = zeroes(ushort, $width, $height);
        for ($j = $height - 1; $j >= 0; $j--) {
        	for ($i = 0; $i < $width; $i++) {
                $pixel = <CCD_DEVICE>;
        		set $pixel_array, ($i, $j), $pixel;
        	}
        }
        until (($header = <CCD_DEVICE>) =~ /IMAGE>/) {}
    }
    
    close CCD_DEVICE;
    return $pixel_array;
}

#
# Needed for module init result.
#
1;

