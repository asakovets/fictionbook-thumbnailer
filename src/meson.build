add_global_arguments('-DTHUMBNAILER_RETURNS_DATA', language : 'c')
add_global_arguments('-DTHUMBNAILER_USAGE="Thumbnail FictionBook ebook"', language : 'c')

prefix = get_option('prefix')
bindir = prefix / get_option('bindir')
thumbnailers_dir = prefix / get_option('datadir') / 'thumbnailers'

configure_file(
        input : 'fictionbook-thumbnailer.thumbnailer.in',
        output : 'fictionbook-thumbnailer.thumbnailer',
        configuration : { 'bindir' : bindir },
        install_dir : thumbnailers_dir)

executable(
        'fictionbook-thumbnailer',
        'fictionbook-thumbnailer.c', 'gnome-thumbnailer-skeleton.c',
        dependencies : deps,
        install : true)
