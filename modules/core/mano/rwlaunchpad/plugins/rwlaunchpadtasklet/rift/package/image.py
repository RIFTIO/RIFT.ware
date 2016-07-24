import re

IMAGE_REGEX = r"{prefix}/?images/(?P<image_name>[^/]+.\.qcow2)$"

def get_package_image_files(package):
    """ Return a image name/file map for images in the descriptor

    Arguments:
        package - A DescriptorPackage

    Returns:
        A dictionary mapping image names to the relative path within
        the package.
    """
    image_file_map = {}

    for file_name in package.files:
        match = re.match(
                IMAGE_REGEX.format(prefix=package.prefix),
                file_name,
                )
        if match is None:
            continue

        image_name = match.group("image_name")
        image_file_map[image_name] = file_name

    return image_file_map
