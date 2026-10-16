/* Included only by the non-installed integration test executable. */
static bool app_run_hardening_tests(void)
{
    app_state app = {0};
    app_auth_task task = {0};
    bool success;

    {
        char first_nonce[LOIM_ENTITLEMENT_NONCE_CAPACITY];
        char second_nonce[LOIM_ENTITLEMENT_NONCE_CAPACITY];
        loim_entitlement cached;
        if (!loim_entitlement_nonce(first_nonce) || !loim_entitlement_nonce(second_nonce) ||
            strlen(first_nonce) != 64U || strcmp(first_nonce, second_nonce) == 0) return false;
        if (loim_entitlement_cache_save("unsigned\n", first_nonce)) return false;
        if (loim_entitlement_cache_load("device", "user@example.com", 1700000000U, &cached)) return false;
    }
    app.licensed = true;
    app.logged_in = true;
    app.export_licensed = true;
    task.restore_session = true;
    task.background_refresh = true;
    task.status = LOIM_ERROR_IO;
    app_apply_background_auth(&app, &task);
    success = app.licensed && app.logged_in && app.export_licensed && !app.login_open;
    task.status = LOIM_ERROR_INVALID_DATA;
    task.result.success = true;
    app_apply_background_auth(&app, &task);
    success = success && app.licensed;
    task.status = LOIM_OK;
    app_apply_background_auth(&app, &task);
    success = success && app.licensed; /* Legacy heartbeat without entitlement details. */
    task.result.success = false;
    task.http_status = 404;
    app_apply_background_auth(&app, &task);
    success = success && app.licensed; /* Generic 404 is not a revocation. */
    task.http_status = 401;
    app_copy_text(task.result.error_code, sizeof(task.result.error_code), "TOKEN_EXPIRED");
    app_apply_background_auth(&app, &task);
    success = success && app.licensed && app.logged_in && app.export_licensed && !app.login_open;
    app_copy_text(task.result.error_code, sizeof(task.result.error_code), "UNAUTHORIZED");
    app_apply_background_auth(&app, &task);
    success = success && app.licensed;
    app_copy_text(task.result.error_code, sizeof(task.result.error_code), "INVALID_TOKEN");
    app_apply_background_auth(&app, &task);
    success = success && app.licensed;
    task.http_status = 404;
    app_copy_text(task.result.error_code, sizeof(task.result.error_code), "DEVICE_NOT_FOUND");
    app_apply_background_auth(&app, &task);
    success = success && !app.licensed && app.logged_in && app.export_licensed && !app.login_open;
    task.result.success = true;
    task.http_status = 200;
    task.result.subscription_active_present = true;
    task.result.subscription_active = true;
    task.entitlement_verified = true;
    app_copy_text(task.result.email, sizeof(task.result.email), "subscriber@example.com");
    app_apply_background_auth(&app, &task);
    success = success && app.licensed;
    task.result.subscription_active = false;
    app_apply_background_auth(&app, &task);
    success = success && !app.licensed && app.export_licensed;
    (void)printf("hardening=%s\n", success ? "pass" : "fail");
    return success;
}
