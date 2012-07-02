<?php 

function motevia_moteviaCurl_info() {
	watchdog("A", "A");
	$source = array();
	$source['moteviaCurl'] = array(
		'name' => t('Remote URL textfield'),
		'label' => t('Remote URL'),
		'description' => t('Download a file from a remote server.'),
		'process' => 'motevia_moteviaCurl_process',
		'value' => 'motevia_moteviaCurl_value',
	);
	return $source;
}

function motevia_moteviaCurl_menu() {
	watchdog("B", "B");
	$items = array();
	
	$items['file/remote/progress/%/%/%/%'] = array(
		'page callback' => 'motevia_moteviaCurl_progress',
		'page arguments' => array(3, 4, 5, 6),
		'access arguments' => array('access content'),
		'file' => 'includes/moteviaCurl.inc',
		'type' => MENU_CALLBACK,
	);
	return $items;
}

function motevia_moteviaCurl_process($element, &$form_state, $form) {
	watchdog("C", "C");
	$element['motevia_moteviaCurl'] = array(
		'#weight' => 100.5,
		'#motevia' => TRUE,
	);

	$element['motevia_moteviaCurl']['url'] = array(
		'#type' => 'textfield',
// 		'#description' => t('URL Description'),
		'#maxlength' => NULL,
	);
	
	$element['motevia_moteviaCurl']['transfer'] = array(
		'#name' => implode('_', $element['#array_parents']) . '_transfer',
		'#type' => 'submit',
		'#value' => t('Transfer'),
		'#validate' => array(),
		'#submit' => array('motevia_field_submit'),
		'#limit_validation_errors' => array($element['#parents']),
		'#ajax' => array(
			'path' => 'file/ajax/' . implode('/', $element['#array_parents']) . '/' . $form['form_build_id']['#value'],
			'wrapper' => $element['#id'] . '-ajax-wrapper',
			'effect' => 'fade',
			'progress' => array(
				'type' => 'bar',
				'path' => 'file/remote/progress/' . $element['#entity_type'] . '/' . $element['#bundle'] . '/' . $element['#field_name'] . '/' . $element['#delta'],
				'message' => t('Starting transfer...'),
			),
		),
	);
	
	return $element;
}

function motevia_moteviaCurl_value($element, &$item) {
	watchdog("D", "D");
	if (isset($item['motevia_moteviaCurl']['url']) && strlen($item['motevia_moteviaCurl']['url']) > 0 && valid_url($item['motevia_moteviaCurl']['url'])) {
		$field = field_info_instance($element['#entity_type'], $element['#field_name'], $element['#bundle']);
		$url = $item['motevia_moteviaCurl']['url'];
		
		$temporary_directory = 'temporary://';
		if (!file_prepare_directory($temporary_directory, FILE_MODIFY_PERMISSIONS)) {
			watchdog('file', 'The directory %directory is not writable, because it does not have the correct permissions set.', array('%directory' => drupal_realpath($temporary_directory)));
			drupal_set_message(t('The file could not be transferred because the temporary directory is not writable.'), 'error');
			return;
		}
		
		$directory = $element['#upload_location'];
		if (!file_prepare_directory($directory, FILE_CREATE_DIRECTORY)) {
			watchdog('file', 'File %file could not be copied, because the destination directory %destination is not configured correctly.', array('%file' => $url, '%destination' => drupal_realpath($directory)));
			drupal_set_message(t('The specified file %file could not be copied, because the destination directory is not properly configured. This may be caused by a problem with file or directory permissions. More information is available in the system log.', array('%file' => $url)), 'error');
			return;
		}
		
		$ch = curl_init();
		curl_setopt($ch, CURLOPT_URL, $url);
		curl_setopt($ch, CURLOPT_HEADER, TRUE);
		curl_setopt($ch, CURLOPT_NOBODY, TRUE);
		curl_setopt($ch, CURLOPT_RETURNTRANSFER, TRUE);
		curl_setopt($ch, CURLOPT_HEADERFUNCTION, '_motevia_moteviaCurl_parse_header');
		@curl_setopt($ch, CURLOPT_FOLLOWLOCATION, TRUE);
		curl_exec($ch);
		$info = curl_getinfo($ch);
		curl_close($ch);
		
		if ($info['http_code'] != 200) {
			switch ($info['http_code']) {
				case 403:
					form_error($element, t('The remote file could not be transfered because access to the file was denied.'));
					break;
				case 404:
					form_error($element, t('The remote file could not be transfered because it was not found.'));
					break;
				default:
					form_error($element, t('The remote file could not be transfered due to an HTTP error (@code).', array('@code' => $info['http_code'])));
			}
			return;
		}
		
		$url = $info['url'];
		$url_info = parse_url($url);
		
		$filename = _motevia_moteviaCurl_filename();
		if (empty($filename)) {
			$filename = rawurldecode(basename($url_info['path']));
		}
		
		$pathinfo = pathinfo($filename);
		$filename = motevia_clean_filename($filename, $field['settings']['file_extensions']);
		$filepath = file_create_filename($filename, $temporary_directory);
		
		if (empty($pathinfo['extension'])) {
			form_error($element, t('The remote URL must be a file and have an extension.'));
			return;
		}
		
		$extensions = $field['settings']['file_extensions'];
		$regex = '/\.('. preg_replace('/[ +]/', '|', preg_quote($extensions)) .')$/i';
		if (!empty($extensions) && !preg_match($regex, $filename)) {
			form_error($element, t('Only files with the following extensions are allowed: %files-allowed.', array('%files-allowed' => $extensions)));
			return;
		}
		
		if (!empty($element['#upload_validators']['file_validate_size'][0])) {
			$max_size = $element['#upload_validators']['file_validate_size'][0];
			$file_size = $info['download_content_length'];
			if ($file_size > $max_size) {
				form_error($element, t('The remote file is %filesize exceeding the maximum file size of %maxsize.', array('%filesize' => format_size($file_size), '%maxsize' => format_size($max_size))));
				return;
			}
		}
		
		$options = array(
				'key' => $element['#entity_type'] . '_' . $element['#bundle'] . '_' . $element['#field_name'] . '_' . $element['#delta'],
				'filepath' => $filepath,
		);
		
		motevia_moteviaCurl_set_transfer_options($options);
		
		$ch = curl_init();
		curl_setopt($ch, CURLOPT_URL, $url);
		curl_setopt($ch, CURLOPT_HEADER, FALSE);
		curl_setopt($ch, CURLOPT_WRITEFUNCTION, 'motevia_moteviaCurl_curl_write');
		@curl_setopt($ch, CURLOPT_FOLLOWLOCATION, TRUE);
		
		if (curl_exec($ch) && $file = motevia_save_file($filepath, $element['#upload_validators'], $element['#upload_location'])) {
			$item = array_merge($item, (array) $file);
		}
		curl_close($ch);
		@unlink($filepath);
	}
}

function _motevia_moteviaCurl_parse_header(&$ch, $header) {
	watchdog("E", "E");
	if (preg_match('/Content-Disposition:.*filename="(.+?)"/', $header, $matches)) {
		_motevia_moteviaCurl_filename($matches[1]);
	}elseif (preg_match('/Content-Disposition:.*?filename=([^;]+)/', $header, $matches)) {
		_motevia_moteviaCurl_filename($matches[1]);
	}
	
	return strlen($header);
}

function _motevia_moteviaCurl_filename($curl_filename = NULL) {
	watchdog("F", "F");
	static $filename = NULL;
	if (isset($curl_filename)) {
		$filename = $curl_filename;
	}
	return $filename;
}

function motevia_moteviaCurl_set_transfer_options($options = NULL) {
	watchdog("G", "G");
	static $current = FALSE;
	if (isset($options)) {
		$current = $options;
	}
	return $current;
}

function motevia_moteviaCurl_curl_write(&$ch, $data) {
	watchdog("H", "H");
	$progress_update = 0;
	$options = motevia_moteviaCurl_get_transfer_options();

	if (curl_getinfo($ch, CURLINFO_SIZE_DOWNLOAD) / 65536 > $progress_update) {
		$progress_update++;
		$progress = array(
				'current' => curl_getinfo($ch, CURLINFO_SIZE_DOWNLOAD),
				'total' => curl_getinfo($ch, CURLINFO_CONTENT_LENGTH_DOWNLOAD),
		);

		$cid = 'filefield_transfer:'. session_id() . ':' . $options['key'];
		if ($progress['current'] != $progress['total']) {
			cache_set($cid, $progress, 'cache', time() + 300);
		}
		else {
			cache_clear_all($cid, 'cache');
		}
	}

	$data_length = 0;
	if ($fp = @fopen($options['filepath'], 'a')) {
		fwrite($fp, $data);
		fclose($fp);
		$data_length = strlen($data);
	}

	return $data_length;
}

function motevia_moteviaCurl_get_transfer_options() {
	watchdog("I", "I");
	return motevia_moteviaCurl_set_transfer_options();
}

function motevia_moteviaCurl_progress($entity_type, $bundle_name, $field_name, $delta) {
	watchdog("J", "J");
	$key = $entity_type . '_' . $bundle_name . '_' . $field_name . '_' . $delta;
	$progress = array(
		'message' => t('Starting transfer...'),
		'percentage' => -1,
	);

	if ($cache = cache_get('filefield_transfer:'. session_id() . ':' . $key)) {
		$current = $cache->data['current'];
		$total = $cache->data['total'];
		$progress['message'] = t('Transfering... (@current of @total)', array('@current' => format_size($current), '@total' => format_size($total)));
		$progress['percentage'] = round(100 * $current / $total);
	}

	drupal_json_output($progress);
}